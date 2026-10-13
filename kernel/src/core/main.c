#include "core/loader.h"
#include "drivers/uart.h"
#include "minemu/boot.h"
#include "minemu/irq.h"
#include "minemu/trap.h"
#include "minemu/trace.h"

void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    static const char module_name[] = "minimum-user";
    if ((uintptr_t)boot_info != MINEMU_BOOT_INFO_VADDR ||
        boot_info->magic != MINEMU_BOOT_INFO_MAGIC ||
        boot_info->version != MINEMU_ABI_VERSION ||
        boot_info->size != sizeof(*boot_info) ||
        boot_info->system_rom_base != UINT32_C(0x08000000) ||
        boot_info->direct_map_vaddr != UINT32_C(0xc0000000) ||
        boot_info->direct_map_paddr != UINT32_C(0x40000000) ||
        boot_info->direct_map_size != UINT32_C(0x04000000)) {
        minemu_trace_event(UINT32_C(0xb007bad0));
        minemu_fail_stop();
    }

    /* Device and controller first, CPU last: nothing may fire before the
     * handler and its buffer are ready. */
    uart_init();
    minemu_irq_enable();

    uint32_t entry = user_load(boot_info, module_name, sizeof(module_name) - 1U);
    if (entry == 0) {
        minemu_trace_event(UINT32_C(0xb007bad1));
        minemu_fail_stop();
    }
    minemu_enter_user(entry, USER_STACK_TOP);
}
