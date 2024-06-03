// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/types.h>
#include <mos/x86/devices/port.h>
#include <mos/x86/x86_platform.h>

// Reinitialize the PIC controllers.
// Giving them specified vector offsets rather than 8h and 70h, as configured by default
// ICW: Initialization command words
#define ICW1_ICW4 0x01 /* ICW4 (not) needed */
#define ICW1_INIT 0x10 /* Initialization - required! */
#define ICW4_8086 0x01 /* 8086/88 (MCS-80/85) mode */

#define PIC1         0x20 // IO base address for master PIC
#define PIC2         0xA0 // IO base address for slave  PIC
#define PIC1_COMMAND (PIC1)
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND (PIC2)
#define PIC2_DATA    (PIC2 + 1)

#define PIC1_OFFSET 0x20
#define PIC2_OFFSET 0x28

// We now have APIC, so PIC is not used anymore, but the above initialization code is still used
void pic_remap_irq(void)
{
    port_outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); // starts the initialization sequence (in cascade mode)
    port_outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    port_outb(PIC1_DATA, PIC1_OFFSET); // ICW2: Master PIC vector offset
    port_outb(PIC2_DATA, PIC2_OFFSET); // ICW2: Slave PIC vector offset

    port_outb(PIC1_DATA, 4); // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    port_outb(PIC2_DATA, 2); // ICW3: tell Slave PIC its cascade identity (0000 0010)

    port_outb(PIC1_DATA, ICW4_8086);
    port_outb(PIC2_DATA, ICW4_8086);

    port_outb(PIC2_DATA, 0xFF); // mask all interrupts on slave PIC
    port_outb(PIC1_DATA, 0xFF); // mask all interrupts on master PIC
}
