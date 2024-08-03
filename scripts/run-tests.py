#!/usr/bin/env python3

# MOS Tests Runner
# This script should be run from the build or the source directory

import argparse
import asyncio
import logging
import os
import signal
from ptyprocess import PtyProcessUnicode


class KernelCommandLine:
    def __init__(self,):
        self.mos_tests: bool = False
        self.printk_console: str | None = None
        self.init: str | None = None

    def to_string(self):
        items = []
        for k, v in self.__dict__.items():
            if v is None:
                continue
            if isinstance(v, bool):
                if v:
                    items.append(k)
            else:
                items.append(f'{k}={v}')
        return ' '.join(items)


class RuntimeConfig:
    def __init__(self) -> None:
        self.sleep_time = 10


LIMINE_CFG_TEMPLATE = """
TIMEOUT=0
:MOS-{arch}
    PROTOCOL=limine
    KERNEL_PATH=boot:///boot/MOS/{arch}-kernel.elf
    MODULE_PATH=boot:///boot/MOS/{arch}-initrd.cpio
    CMDLINE={cmdline}
"""

CONFIG = RuntimeConfig()
CMDLINE = KernelCommandLine()

QEMU_ARCH_ARGS = {
    'x86_64': {
        'machine': 'q35',
        'cpu': 'max',
        'bios': '/usr/share/ovmf/x64/OVMF.4m.fd',
    },
    'riscv64': {
        'machine': 'virt',
        'cpu': 'TODO',
        'bios': 'TODO',
    },
}


class QemuProcess(PtyProcessUnicode):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def kill(self):
        print('Killing QEMU process')
        super().kill(signal.SIGKILL)

    def sleep(self):
        return asyncio.sleep(CONFIG.sleep_time)

    def run_command(self, command):
        logging.debug(f'Running command: {command}')
        self.write(command + '\n')
        return asyncio.sleep(2)

    def graceful_shutdown(self):
        self.write('shutdown\n')


class QemuProcessBuilder:
    def __init__(self, qemu_path: str):
        self.qemu_path = qemu_path
        self.args = [self.qemu_path]
        self.process: QemuProcess

    def add_raw_arg(self, arg, value=None):
        self.args.append(arg)
        if value:
            self.args.append(str(value))

    def machine(self, machine):
        self.add_raw_arg('-machine', machine)

    def memory(self, memory):
        self.add_raw_arg('-m', memory)

    def smp(self, N: str):
        self.add_raw_arg('-smp', N)

    def cpu(self, cpu: str):
        self.add_raw_arg('-cpu', cpu)

    def bios(self, bios: str):
        self.add_raw_arg('-bios', bios)

    def accel(self, accel: str):
        self.add_raw_arg('-accel', accel)

    def chardev(self, backend: str, **kwargs):
        self.add_raw_arg('-chardev', backend + ',' + ','.join([f'{k}={v}' for k, v in kwargs.items()]))

    def serial(self, serial):
        self.add_raw_arg('-serial', serial)

    def drive(self, **kwargs):
        self.add_raw_arg('-drive', ','.join([f'{k}={v}' for k, v in kwargs.items()]))

    def start(self):
        self.process = QemuProcess.spawn(self.args)
        return self.process


async def main() -> int:
    logging.basicConfig(level=logging.INFO, style='{', format='{asctime} - {levelname} - {message}')
    parser = argparse.ArgumentParser()
    parser.add_argument('-a', '--arch', help='The architecture to run the test on', default=os.uname().machine)
    parser.add_argument('-m', '--memory-size', help='The memory size to use for the test', default='4G')
    parser.add_argument('-c', '--cpu-count', help='The number of CPUs to use for the test', default=1)
    parser.add_argument('-t', '--timeout', help='The timeout for the test', default=60)
    parser.add_argument('--gui', help='Display QEMU GUI', action='store_true', default=False)
    parser.add_argument('--serial-log-file', help='The file to log the serial output to', default="test-results/serial.log")
    parser.add_argument('--kernel-log-file', help='The file to log the kernel output to', default="test-results/kernel.log")
    parser.add_argument('--kvm', help='Enable KVM', action='store_true')
    parser.add_argument('--kernelspace-tests', help='Run the kernel space tests', action='store_true')
    args = parser.parse_args()

    # check if we are in the build directory
    if not os.path.exists('./uefi-files'):
        # try to change to the build directory
        try:
            os.chdir(f'./build/{args.arch}')
        except FileNotFoundError:
            print('Please run this script from the build directory')
            return 1

    if not os.path.exists('test-results'):
        logging.info('Created directory for test results')
        os.mkdir('test-results')

    builder = QemuProcessBuilder(f'qemu-system-{args.arch}')
    builder.memory(args.memory_size)
    builder.smp(args.cpu_count)
    builder.machine(QEMU_ARCH_ARGS[args.arch]['machine'])
    builder.cpu(QEMU_ARCH_ARGS[args.arch]['cpu'])
    builder.bios(QEMU_ARCH_ARGS[args.arch]['bios'])
    builder.chardev("stdio", id='serial0', logfile=args.serial_log_file)
    builder.chardev('file', id='syslog', path=args.kernel_log_file)
    builder.serial('chardev:serial0')
    builder.serial('chardev:syslog')
    builder.drive(format='raw', file='fat:rw:uefi-files/')

    if args.kvm:
        builder.accel('kvm')
        logging.info('KVM enabled')
        CONFIG.sleep_time = 5  # KVM is faster

    if not args.gui:
        builder.add_raw_arg('-display', 'none')

    CMDLINE.printk_console = 'serial_com2'
    CMDLINE.mos_tests = args.kernelspace_tests

    logging.info(f'Using cmdline: {CMDLINE.to_string()}')

    with open('uefi-files/boot/limine/limine.cfg', 'w') as f:
        # Apply cmdline options
        content = LIMINE_CFG_TEMPLATE.format(arch=args.arch, cmdline=CMDLINE.to_string())
        logging.debug('Writing limine.cfg: ' + content)
        f.write(content)

    # open qemu in a subprocess with separate pty
    logging.info('QEMU arguments: ' + ' '.join(builder.args))
    qemu_io = builder.start()

    try:
        async with asyncio.timeout(args.timeout):
            wait_task = asyncio.to_thread(lambda: qemu_io.wait())

            await qemu_io.sleep()  # wait for QEMU to boot
            await qemu_io.run_command('test-launcher')
            qemu_io.graceful_shutdown()

            await wait_task
            print('QEMU process finished')
    except asyncio.TimeoutError:
        print('Timeout reached, killing QEMU process')
        qemu_io.kill()
        return 1

    return 0

if __name__ == '__main__':
    loop = asyncio.new_event_loop()
    ret = loop.run_until_complete(main())
    exit(ret)
