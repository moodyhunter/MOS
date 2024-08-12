# SPDX-License-Identifier: GPL-3.0-or-later

from abc import ABC, abstractmethod
import json
import logging
from typing import Literal

from utils import QemuProcess, QemuDeadError


_default_encoder = json.JSONEncoder().default


def wrapped_default(self, obj):
    return getattr(obj.__class__, "__json__", _default_encoder)(obj)


json.JSONEncoder.default = wrapped_default   # type: ignore


class AutoRepr:
    def __repr__(self) -> str:
        return f"{type(self).__qualname__}({', '.join([f"{k}: {repr(v)}" for k, v in self.__dict__.items()])})"

    def __str__(self) -> str:
        return self.__repr__()


def do_call(qemu: QemuProcess, rpc_type: str, data: dict) -> dict:
    qemu.writeln(json.dumps({'type': rpc_type + '.request', 'object': data}))
    logging.debug(f'Sent request: {rpc_type}, {data}')

    while True:
        line = qemu.pop_output()
        if line is None:
            return {}  # no more output

        if not (line.startswith('{') and line.endswith('}')):
            continue  # ignore non-json lines

        obj = json.loads(line)
        resp_type, resp_obj = obj['type'], obj['object']

        if resp_type == rpc_type + '.request':
            continue  # ignore our request
        if resp_type == rpc_type + '.response':
            return resp_obj
        else:
            logging.error(f'Unexpected response type: {obj["type"]}')


class UnitTestCase(ABC):
    class Response:  # make pylance happy
        def __init__(self, request: 'UnitTestCase', **kwargs) -> None:
            ...

    @property
    @abstractmethod
    def rpc_type(self):
        raise NotImplementedError

    def call(self, qemu: QemuProcess) -> Response | None:
        try:
            resp_dict = do_call(qemu, self.rpc_type, self.__dict__)
            return self.Response(self, **resp_dict) if resp_dict else None
        except QemuDeadError:
            return None


REDIRECT_TARGET_TYPE = Literal['file', 'fd']


class RunCommand(AutoRepr, UnitTestCase):
    @property
    def rpc_type(self):
        return 'run-command'

    class RedirectionEntry(AutoRepr):
        def __init__(self, type: REDIRECT_TARGET_TYPE, item: str | int, *, read: bool = False, write: bool = False, append: bool = False) -> None:
            self.read: bool = read
            self.write: bool = write
            self.append: bool = append
            self.type: REDIRECT_TARGET_TYPE = type

            if type == 'file':
                self.target = str(item)
            elif type == 'fd':
                self.target = int(item)

        def __json__(self):
            return self.__dict__

    def __init__(self, command: str, args: list[str] = []) -> None:
        self.command = command
        self.argv = [command] + args  # argv[0] is the program name
        self.redirections: dict[int, RunCommand.RedirectionEntry] = {}
        self.return_stdout: bool = True
        self.return_stderr: bool = True

    def redirect(self, fd: int, target: str, *, read: bool = False, write: bool = False, append: bool = False) -> 'RunCommand':
        self.redirections[fd] = self.RedirectionEntry('file', target, read=read, write=write, append=append)
        return self

    class Response(AutoRepr):
        def __init__(self, request: 'RunCommand', returncode: int) -> None:
            self.returncode = returncode


class ReadFile(AutoRepr, UnitTestCase):
    @property
    def rpc_type(self):
        return 'read-file'

    def __init__(self, path: str, save_to: str = '') -> None:
        self.path = path
        self.save_to = save_to

    class Response(AutoRepr):
        def __init__(self, request: 'ReadFile', content: str) -> None:
            self.content = content
            if request.save_to:
                with open(request.save_to, 'w') as f:
                    f.write(content)


class WriteFile(AutoRepr, UnitTestCase):
    @property
    def rpc_type(self):
        return 'write-file'

    def __init__(self, path: str, content: str) -> None:
        self.path = path
        self.content = content

    class Response(AutoRepr):
        def __init__(self, request: 'WriteFile', written: int) -> None:
            self.written = written


class Shutdown(AutoRepr, UnitTestCase):
    @property
    def rpc_type(self):
        return 'shutdown'

    def __init__(self) -> None:
        self.stub = 'stub'
        pass

    class Response(AutoRepr):
        def __init__(self, request: 'Shutdown', stub: str) -> None:
            self.stub = stub
            pass


class Sleep(AutoRepr, UnitTestCase):
    @property
    def rpc_type(self):
        return 'sleep'

    def __init__(self, seconds: float) -> None:
        self.seconds = seconds

    class Response(AutoRepr):
        def __init__(self, request: 'Sleep') -> None:
            pass
