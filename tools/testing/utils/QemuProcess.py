# SPDX-License-Identifier: GPL-3.0-or-later

import logging
import re
import threading
from time import sleep

from ptyprocess import PtyProcess


class QemuDeadError(Exception):
    pass


ANSI_REGEX_MATCH = r'(\x9B|\x1B\[)[0-?]*[ -\/]*[@-~]'


QEMU_ARCH_ARGS = {
    'x86_64': {
        'machine': 'q35',
        'cpu': 'max',
        'bios': '/usr/share/ovmf/x64/OVMF.4m.fd',
    },
}


class QemuProcess(PtyProcess):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.error_killed = False
        self.output: list[str] = []

    def _encode_command(self, command):
        return command.encode('utf-8')

    def writeln(self, line):
        if not self.isalive():
            raise QemuDeadError('Process is terminated')
        logging.debug(f'sending line: {line}')
        self.write(self._encode_command(line + '\n'))

    def do_read_qemu_buf(self):
        while self.isalive():
            try:
                line = self.readline()
                line = re.sub(ANSI_REGEX_MATCH, '', line.decode('utf-8'), flags=re.M)  # remove ANSI escape sequences
                line = line.strip()
                self.output.append(line)
            except EOFError:
                break

    def start_read_qemu_buf(self) -> threading.Thread:
        t = threading.Thread(target=self.do_read_qemu_buf)
        t.start()
        return t

    def pop_output(self) -> str | None:
        while self.isalive():
            try:
                return self.output.pop(0)
            except IndexError:
                sleep(0.1)

        raise QemuDeadError('Process is terminated')

    def forcefully_terminate(self):
        self.error_killed = True

        if not self.isalive():
            return

        try:
            logging.info("terminating qemu process")
            if not self.terminate():
                logging.warning("Process did not terminate, killing it")
                if not self.terminate(force=True):
                    logging.error("Failed to kill process")
                else:
                    logging.info("Process killed")
            else:
                logging.debug("Process terminated")
        except Exception as e:
            import traceback
            traceback.print_exc()
            print(f"Error terminating process: {e}")


class QemuProcessBuilder:
    QEMU_ARGS = {
        'machine': '-machine',
        'memory': '-m',
        'smp': '-smp',
        'cpu': '-cpu',
        'bios': '-bios',
        'accel': '-accel',
        'serial': '-serial',
        'display': '-display',
    }

    def __init__(self, qemu_path: str):
        self.qemu_path = qemu_path
        self.args = [self.qemu_path]
        self.process: QemuProcess

    def __getattr__(self, name):
        if name in self.QEMU_ARGS:
            return lambda value: self.add_raw_arg(self.QEMU_ARGS[name], value)
        raise AttributeError(f'QemuProcessBuilder has no attribute {name}')

    def add_raw_arg(self, arg, value: str | None = None):
        self.args.append(arg)
        if value:
            self.args.append(str(value))

    def _format_kwargs(self, kwargs):
        return ','.join([f'{k}={v}' for k, v in kwargs.items()])

    def chardev(self, backend: str, **kwargs):
        self.add_raw_arg('-chardev', backend + ',' + self._format_kwargs(kwargs))

    def drive(self, **kwargs):
        self.add_raw_arg('-drive', self._format_kwargs(kwargs))

    def start_qemu(self):
        self.process = QemuProcess.spawn(self.args)
        return self.process
