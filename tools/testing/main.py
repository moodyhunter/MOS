#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

# MOS Tests Runner
# This script should be run from the build or the source directory

import argparse
import logging
import os
from time import sleep
from utils import ScopedTimer, QEMU_ARCH_ARGS, QemuDeadError, QemuProcessBuilder
from models import *


class KernelCommandLine:
    def __init__(self,):
        self.mos_tests: bool = False
        self.printk_console: str | None = None
        self.init: str | None = None
        self.init_args: str | None = None
        self.debug: list[str] = []

    def to_string(self):
        items: list[str] = []
        for k, v in self.__dict__.items():
            if k == 'debug':
                assert isinstance(v, list)
                debug_list: list[str] = v
                for e in debug_list:
                    if e.startswith('!'):
                        items.append(f'debug.{e[1:]}=0')
                    else:
                        items.append(f'debug.{e}=1')
                continue

            if v is None:
                continue

            if isinstance(v, bool):
                if v:
                    items.append(str(int(k)))
            elif isinstance(v, str):
                items.append(f'{k}={v}')
            else:
                raise ValueError(f'Unsupported type {type(v)} for {k}')

        return ' '.join(items)


LIMINE_CONF_TEMPLATE = """
timeout: 0

/limine MOS-{arch}
    protocol: limine
    kernel_path: boot():/boot/MOS/{arch}-kernel.elf
    module_path: boot():/boot/MOS/{arch}-initrd.cpio
    cmdline: {cmdline}
"""

RESULTS_DIR = 'test-results/'


def main():
    logging.basicConfig(level=logging.INFO, style='{', format='{asctime} - {levelname} - {message}')
    parser = argparse.ArgumentParser()
    parser.add_argument('-a', '--arch', help='The architecture to run the test on', default=os.uname().machine)
    parser.add_argument('-m', '--memory-size', help='The memory size to use for the test', default='4G')
    parser.add_argument('--smp', help='The number of CPUs to use for the test', default=1)
    parser.add_argument('-t', '--timeout', help='The timeout for the test', default=60, type=int)
    parser.add_argument('-w', '--boot-wait', help='Wait for user input before starting the test', default=5, type=int)
    parser.add_argument('--gui', help='Display QEMU GUI', action='store_true', default=False)
    parser.add_argument('--serial-log-file', help='The file to log the serial output to', default=RESULTS_DIR+"serial.log")
    parser.add_argument('--kernel-log-file', help='The file to log the kernel output to', default=RESULTS_DIR+"kernel.log")
    parser.add_argument('--kvm', help='Enable KVM', action='store_true')
    parser.add_argument('--kernel-tests', help='Run the kernel space tests', action='store_true')
    parser.add_argument('--kernel-debug', help='Enable kernel debug', default=[], type=lambda t: [s.strip() for s in t.split(',')])
    parser.add_argument('--gdbstub', help='Enable GDB stub', action='store_true')
    parser.add_argument('-d', '--dump-serial', help='Dump serial output to stdout', action='store_true')
    args = parser.parse_args()

    logging.info(f'Test configuration: {args}')

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

    if not os.path.exists(RESULTS_DIR):
        logging.info('Created directory for test results')
        os.mkdir(RESULTS_DIR)

    builder = QemuProcessBuilder(f'qemu-system-{args.arch}')
    builder.memory(args.memory_size)
    builder.smp(args.smp)
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
        builder.display('none')

    if args.gdbstub:
        builder.add_raw_arg('-s')
        builder.add_raw_arg('-S')

    CMDLINE = KernelCommandLine()
    CMDLINE.printk_console = 'serial_com2'
    CMDLINE.mos_tests = args.kernel_tests
    CMDLINE.init_args = '-j'
    CMDLINE.debug.extend(args.kernel_debug)

    commandline = CMDLINE.to_string()
    logging.info(f'Using cmdline: {commandline}')

    with open('uefi-files/boot/limine/limine.conf', 'w') as f:
        # Apply cmdline options
        content = LIMINE_CONF_TEMPLATE.format(arch=args.arch, cmdline=commandline)
        logging.debug('Writing limine.conf: ' + content)
        f.write(content)

    # open qemu in a subprocess with separate pty
    logging.info('QEMU arguments: ' + ' '.join(builder.args))
    QEMU_IO = builder.start_qemu()
    work = QEMU_IO.start_read_qemu_buf()

    with ScopedTimer(args.timeout, QEMU_IO.forcefully_terminate):
        try:
            logging.info('Waiting for QEMU to boot...')
            sleep(args.boot_wait)  # wait for QEMU to boot

            logging.info('Starting tests...')

            TESTS: list[UnitTestCase] = [
                RunCommand('export', ['PATH=/initrd/programs:/initrd/bin:/initrd/tests']),
                RunCommand('mkdir', ['/test-results']),
                RunCommand('test-launcher').redirect(1, '/test-results/test-launcher.log', write=True),
                ReadFile('/test-results/test-launcher.log', save_to=RESULTS_DIR + 'test-launcher.log'),
            ]

            for test in TESTS:
                response = test.call(QEMU_IO)
                if response is not None:
                    logging.info(f'Received response: {response}')
                else:
                    logging.error(f'Test {test} failed')
                sleep(1)

            logging.info('Test completed, waiting for QEMU to shutdown...')
            Shutdown().call(QEMU_IO)
            QEMU_IO.wait()

            if QEMU_IO.error_killed:
                logging.error(f'Timed out after {args.timeout} seconds')
                return 1
        except QemuDeadError:
            logging.error('QEMU process terminated unexpectedly')
        except KeyboardInterrupt:
            logging.error('Interrupted by user')
        finally:
            QEMU_IO.forcefully_terminate()
            QEMU_IO.wait()
            work.join()

    return 0


exit(main())
