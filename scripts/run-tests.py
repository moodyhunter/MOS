#!/usr/bin/env python3

# MOS Tests Runner
# This script should be run from the build or the source directory

import argparse
import asyncio
import logging
from multiprocessing import Process
import os
import signal
import threading
from time import sleep
from typing import final
from ptyprocess import PtyProcess


class QemuDeadError(Exception):
    pass


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


LIMINE_CONF_TEMPLATE = """
timeout: 0

/limine MOS-{arch}
    protocol: limine
    kernel_path: boot():/boot/MOS/{arch}-kernel.elf
    module_path: boot():/boot/MOS/{arch}-initrd.cpio
    cmdline: {cmdline}
"""

CMDLINE = KernelCommandLine()

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
        self.work = threading.Thread(target=self.discard_readbuf)
        self.work.start()
        self.error_killed = False

    def _encode_command(self, command):
        return command.encode('utf-8')

    def run_command(self, command):
        if not self.isalive():
            raise QemuDeadError('Process is terminated')
        logging.debug(f'Running command: {command}')
        self.write(self._encode_command(command + '\n'))

    def graceful_shutdown(self):
        if not self.isalive():
            raise QemuDeadError('Process is terminated')
        self.write(self._encode_command('shutdown\n'))

    def discard_readbuf(self):
        while self.isalive():
            try:
                self.read()
            except EOFError:
                break

    def forcefully_terminate(self):
        self.error_killed = True

        if not self.isalive():
            return

        try:
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
        finally:
            self.work.join()


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


def main():
    logging.basicConfig(level=logging.INFO, style='{', format='{asctime} - {levelname} - {message}')
    parser = argparse.ArgumentParser()
    parser.add_argument('-a', '--arch', help='The architecture to run the test on', default=os.uname().machine)
    parser.add_argument('-m', '--memory-size', help='The memory size to use for the test', default='4G')
    parser.add_argument('-c', '--cpu-count', help='The number of CPUs to use for the test', default=1)
    parser.add_argument('-t', '--timeout', help='The timeout for the test', default=60, type=int)
    parser.add_argument('-w', '--wait', help='Wait for user input before starting the test', default=5, type=int)
    parser.add_argument('--gui', help='Display QEMU GUI', action='store_true', default=False)
    parser.add_argument('--serial-log-file', help='The file to log the serial output to', default="test-results/serial.log")
    parser.add_argument('--kernel-log-file', help='The file to log the kernel output to', default="test-results/kernel.log")
    parser.add_argument('--kvm', help='Enable KVM', action='store_true')
    parser.add_argument('--kernelspace-tests', help='Run the kernel space tests', action='store_true')
    parser.add_argument('--gdbstub', help='Enable GDB stub', action='store_true')
    args = parser.parse_args()

    if args.arch not in QEMU_ARCH_ARGS:
        print('Unsupported architecture, skipping tests')
        return 0

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

    if not args.gui:
        builder.add_raw_arg('-display', 'none')

    if args.gdbstub:
        builder.add_raw_arg('-s')
        builder.add_raw_arg('-S')

    CMDLINE.printk_console = 'serial_com2'
    CMDLINE.mos_tests = args.kernelspace_tests

    commandline = CMDLINE.to_string()

    logging.info(f'Using cmdline: {commandline}')

    with open('uefi-files/boot/limine/limine.conf', 'w') as f:
        # Apply cmdline options
        content = LIMINE_CONF_TEMPLATE.format(arch=args.arch, cmdline=commandline)
        logging.debug('Writing limine.conf: ' + content)
        f.write(content)

    # open qemu in a subprocess with separate pty
    logging.info('QEMU arguments: ' + ' '.join(builder.args))
    QEMU_IO = builder.start()

    # Create a timer to terminate the process if it runs too long
    timer = threading.Timer(args.timeout, QEMU_IO.forcefully_terminate)

    try:
        timer.start()

        logging.info('Waiting for QEMU to boot...')
        sleep(args.wait)  # wait for QEMU to boot

        logging.info('Starting tests...')
        QEMU_IO.run_command('test-launcher')

        sleep(2)
        QEMU_IO.graceful_shutdown()

        logging.info('Test completed, waiting for QEMU to shutdown...')
        QEMU_IO.wait()

        if QEMU_IO.error_killed:
            logging.error(f'Timed out after {args.timeout} seconds')
            return 1
    finally:
        timer.cancel()
        QEMU_IO.forcefully_terminate()
        QEMU_IO.wait()

    return 0


try:
    exit(main())
except KeyboardInterrupt:
    import traceback
    traceback.print_exc()
    print('Exiting...')
    exit(1)
