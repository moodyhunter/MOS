// SPDX-License-Identifier

#include "mos/interrupt/interrupt.hpp"
#include "mos/io/io.hpp"
#include "mos/ipc/pipe.hpp"
#include "mos/kmod/kmod-decl.hpp"
#include "mos/kmod/kmod.hpp"
#include "mos/platform/platform.hpp"
#include "mos/tasks/process.hpp"
#include "mos/x86/devices/port.hpp"
#include "mos/x86/interrupt/apic.hpp"
#include "mos/x86/x86_interrupt.hpp"

#include <libipc/ipc.h>
#include <mos/filesystem/fs_types.h>
#include <pb_encode.h>

static IO *writer = nullptr;
static constexpr u32 EVENT_TYPE_KEYBOARD = 1;
static constexpr u32 EVENT_TYPE_MOUSE = 2;

static void ps2_mouse_write(uint8_t a_write)
{
    port_outb(0x64, 0xD4);
    port_outb(0x60, a_write);
}

static void ps2_mouse_init()
{
    // Initialize the PS/2 mouse
    port_outb(0x64, 0xA8); // Enable the mouse
    port_outb(0x64, 0x20); // Read command byte
    u8 command_byte = port_inb(0x60);
    command_byte |= 0x02;  // Enable mouse
    port_outb(0x64, 0x60); // Write command byte
    port_outb(0x60, command_byte);

    ps2_mouse_write(0xF6);
    if (u8 response = port_inb(0x60); response != 0xFA)
    {
        pr_warn("Mouse initialization failed, response: %x", response);
        return;
    }

    ps2_mouse_write(0xF4); // Enable mouse interrupts
    if (u8 response = port_inb(0x60); response != 0xFA)
    {
        pr_warn("Failed to enable mouse interrupts, response: %x", response);
        return;
    }
}

static long do_write_keyboard_event(int scancode)
{
    if (writer == nullptr)
    {
        pr_warn("Keyboard writer is not initialized, cannot write scancode %x", scancode);
        return -ENOTCONN;
    }

    if ((long) writer->write(&EVENT_TYPE_KEYBOARD, sizeof(EVENT_TYPE_KEYBOARD)) < 0)
    {
        pr_warn("Failed to write keyboard event type");
        return -EIO;
    }

    if ((long) writer->write(&scancode, sizeof(scancode)) < 0)
    {
        pr_warn("Failed to write scancode %x", scancode);
        return -EIO;
    }

    return 0;
}

static long do_write_mouse_event(const u8 data[3])
{
    if (writer == nullptr)
    {
        pr_warn("Mouse writer is not initialized, cannot write mouse event");
        return -ENOTCONN;
    }

    if ((long) writer->write(&EVENT_TYPE_MOUSE, sizeof(EVENT_TYPE_MOUSE)) < 0)
    {
        pr_warn("Failed to write mouse event type");
        return -EIO;
    }

    if ((long) writer->write(data, sizeof(u8) * 3) < 0)
    {
        pr_warn("Failed to write mouse data");
        return -EIO;
    }

    return 0;
}

static bool x86_keyboard_handler(u32 irq, void *data)
{
    MOS_UNUSED(data);
    MOS_ASSERT(irq == IRQ_KEYBOARD);
    int scancode = port_inb(0x60);

    if (const auto ret = do_write_keyboard_event(scancode); ret != 0)
        pr_warn("Failed to write keyboard event: %d", ret);

    return true;
}

static bool ps2_mouse_irq_handler(u32 irq, void *data)
{
    MOS_UNUSED(data);
    MOS_ASSERT(irq == IRQ_PS2_MOUSE);

    // Read mouse data from the PS/2 mouse port
    u8 status = port_inb(0x64);
    if (status & 0x01) // Data is available
    {
        u8 data[3];
        data[0] = port_inb(0x60);
        data[1] = port_inb(0x60);
        data[2] = port_inb(0x60);

        if (const auto ret = do_write_mouse_event(data); ret != 0)
            pr_warn("Failed to write mouse event: %d", ret);
    }
    return true;
}

static long do_enable(void *, size_t)
{
    ps2_mouse_init();
    return 0;
}

static long do_subscribe(void *, size_t)
{
    ioapic_enable_interrupt(IRQ_KEYBOARD, platform_info->boot_cpu_id);
    ioapic_enable_interrupt(IRQ_PS2_MOUSE, platform_info->boot_cpu_id);

    auto pipe = pipe_create(MOS_PAGE_SIZE * 4);
    if (pipe.isErr())
        return pipe.getErr();
    auto pipeio = pipeio_create(pipe.get());
    writer = &pipeio->io_w;
    return process_attach_ref_fd(current_process, &pipeio->io_r, FDFlag::FD_FLAGS_NONE);
}

static void kmodentry(ptr<mos::kmods::Module> self)
{
    interrupt_handler_register(IRQ_KEYBOARD, x86_keyboard_handler, NULL);
    interrupt_handler_register(IRQ_PS2_MOUSE, ps2_mouse_irq_handler, NULL);
    self->ExportFunction("subscribe", do_subscribe);
    self->ExportFunction("enable", do_enable);
}

KMOD_AUTHOR("MOS Developers");
KMOD_DESCRIPTION("Input device driver for MOS");
KMOD_ENTRYPOINT(kmodentry);
