// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mm/paging/paging.h>
#include <mos/mm/physical/pmm.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/x86/acpi/madt.h>
#include <mos/x86/interrupt/apic.h>
#include <mos/x86/x86_interrupt.h>
#include <mos/x86/x86_platform.h>

// +-------+     +-------+     +-------+
// |       |     |       |     |       |
// | CPU 0 |     | CPU 1 |     | CPU 2 |  ...
// |       |     |       |     |       |
// +-------+     +-------+     +-------+
// | LAPIC |     | LAPIC |     | LAPIC |  ...
// +---+---+     +---+---+     +---+---+
//     |             |             |
//     |      +------+             |
//     +---+  |  +-----------------+
//         |  |  |
//         |  |  |
//      +--v--v--v---+
//      |  I/O APIC  | <- Interrupts are sent to this
//      +------------+

#define IOAPIC_REG_ID             0x00
#define IOAPIC_REG_VERSION        0x01
#define IOAPIC_REG_ARB_ID         0x02
#define IOAPIC_REG_REDIR_TABLE(n) (0x10 + (n) *2)

typedef struct
{
    u8 interrupt_vec : 8;
    u8 delivery_mode : 3;                   // 0 = Normal, 1 = Low Priority, 2 = SMI, 4 = NMI, 5 = INIT, 7 = External
    bool destination_mode : 1;              // 0 = Physical, 1 = Logical
    bool pending : 1;                       // 1 = if this interrupt is going to be sent, but the APIC is busy
    ioapic_polarity_t polarity : 1;         // 0 = Active High, 1 = Active Low
    bool something : 1;                     // 0 = a local APIC has sent an EOI, 1 = a local APIC has received the interrupt
    ioapic_trigger_mode_t trigger_mode : 1; // 0 = Edge, 1 = Level
    bool mask : 1;                          // 0 = Enabled, 1 = Disabled
    u64 reserved : 39;
    struct
    {
        u8 target_apic_id : 4;
        u8 something_else : 4;
    } __packed destination;
} __packed ioapic_redirection_entry_t;

MOS_STATIC_ASSERT(sizeof(ioapic_redirection_entry_t) == sizeof(u64), "ioapic_register_1 is not 64 bits");

static u32 volatile *ioapic = NULL;

should_inline u32 ioapic_read(u32 reg)
{
    ioapic[0] = reg & 0xff;
    return ioapic[4];
}

should_inline void ioapic_write(u32 reg, u32 value)
{
    ioapic[0] = reg & 0xff;
    ioapic[4] = value;
}

should_inline void ioapic_write_redirection_entry(u32 irq, ioapic_redirection_entry_t entry)
{
    union
    {
        u64 value;
        ioapic_redirection_entry_t entry;
    } __packed u = { .entry = entry };

    ioapic_write(IOAPIC_REG_REDIR_TABLE(irq), u.value & 0xffffffff);
    ioapic_write(IOAPIC_REG_REDIR_TABLE(irq) + 1, u.value >> 32);
}

should_inline ioapic_redirection_entry_t ioapic_read_redirection_entry(u32 irq)
{
    union
    {
        u64 value;
        ioapic_redirection_entry_t entry;
    } __packed u = { 0 };

    u.value = ioapic_read(IOAPIC_REG_REDIR_TABLE(irq));
    u.value |= (u64) ioapic_read(IOAPIC_REG_REDIR_TABLE(irq) + 1) << 32;

    return u.entry;
}

void ioapic_init(void)
{
    MOS_ASSERT_X(x86_ioapic_address != 0, "ioapic: no ioapic found in madt");
    ioapic = (u32 volatile *) x86_ioapic_address;
    if (!pmm_find_reserved_region(x86_ioapic_address))
    {
        pr_info("reserving ioapic address");
        pmm_reserve_address(x86_ioapic_address);
    }

    mm_map_pages(x86_platform.kernel_mm.pagetable, x86_ioapic_address, x86_ioapic_address / MOS_PAGE_SIZE, 1, VM_RW);
    const u32 ioapic_id = ioapic_read(IOAPIC_REG_ID) >> 24 & 0xf; // get the 24-27 bits

    const union
    {
        u32 value;
        struct
        {
            u32 version : 8; // 0 - 7
            u32 reserved : 8;
            u32 max_redir_entries : 8; // 16 - 23
            u32 reserved2 : 8;
        } __packed;
    } version = { .value = ioapic_read(IOAPIC_REG_VERSION) };

    const u32 arb_id = ioapic_read(IOAPIC_REG_ARB_ID) >> 24 & 0xf; // get the 24-27 bits

    mos_debug(x86_ioapic, "max IRQs: %d, id: %d, version: %d, arb: %d", version.max_redir_entries + 1, ioapic_id, version.version, arb_id);

    for (int i = 0; i < version.max_redir_entries + 1; i++)
        ioapic_disable(i);
}

void ioapic_enable_with_mode(u32 irq, u32 cpu, ioapic_trigger_mode_t trigger_mode, ioapic_polarity_t polarity)
{
    mos_debug(x86_ioapic, "enable irq %d, cpu %d, trigger_mode %d, polarity %d", irq, cpu, trigger_mode, polarity);

    ioapic_redirection_entry_t entry = { 0 };
    entry.interrupt_vec = irq + ISR_MAX_COUNT; // the vector number received by the CPU
    entry.polarity = polarity;
    entry.trigger_mode = trigger_mode;
    entry.destination.target_apic_id = cpu;

    u32 irq_overrided = x86_ioapic_get_irq_override(irq); // the irq number received by the ioapic "pin"
    ioapic_write_redirection_entry(irq_overrided, entry);
}

void ioapic_disable(u32 irq)
{
    ioapic_redirection_entry_t entry = { 0 };
    entry.interrupt_vec = irq + ISR_MAX_COUNT;
    entry.mask = true;
    ioapic_write_redirection_entry(irq, entry);
}
