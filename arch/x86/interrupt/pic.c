// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/interrupt/pic.h"

#include "mos/types.h"
#include "mos/x86/devices/port.h"
#include "mos/x86/x86_platform.h"

// Reinitialize the PIC controllers.
// Giving them specified vector offsets rather than 8h and 70h, as configured by default
// ICW: Initialization command words
#define ICW1_ICW4      0x01 /* ICW4 (not) needed */
#define ICW1_SINGLE    0x02 /* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04 /* Call address interval 4 (8) */
#define ICW1_LEVEL     0x08 /* Level triggered (edge) mode */
#define ICW1_INIT      0x10 /* Initialization - required! */

#define ICW4_8086       0x01 /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO       0x02 /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE  0x08 /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C /* Buffered mode/master */
#define ICW4_SFNM       0x10 /* Special fully nested (not) */

#define PIC1         0x20 // IO base address for master PIC
#define PIC2         0xA0 // IO base address for slave  PIC
#define PIC1_COMMAND (PIC1)
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND (PIC2)
#define PIC2_DATA    (PIC2 + 1)

// We now have APIC, so PIC is not used anymore, but the above initialization code is still used
void pic_remap_irq(int offset_master, int offset_slave)
{
    port_outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); // starts the initialization sequence (in cascade mode)
    port_outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    port_outb(PIC1_DATA, offset_master); // ICW2: Master PIC vector offset
    port_outb(PIC2_DATA, offset_slave);  // ICW2: Slave PIC vector offset

    port_outb(PIC1_DATA, 4); // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    port_outb(PIC2_DATA, 2); // ICW3: tell Slave PIC its cascade identity (0000 0010)

    port_outb(PIC1_DATA, ICW4_8086);
    port_outb(PIC2_DATA, ICW4_8086);

    port_outb(PIC2_DATA, 0xFF); // mask all interrupts on slave PIC
    port_outb(PIC1_DATA, 0xFF); // mask all interrupts on master PIC
}
