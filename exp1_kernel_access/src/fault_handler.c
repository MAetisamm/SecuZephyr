#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/fatal.h>
#include <cmsis_core.h>

static uint32_t saved_mmfar;
static uint32_t saved_cfsr;

void k_sys_fatal_error_handler(unsigned int reason,
                                const struct arch_esf *esf)
{
    /* Read fault registers immediately — before they get cleared */
    uint32_t mmfar = SCB->MMFAR;
    uint32_t cfsr  = SCB->CFSR;

    printk("\n========================================\n");
    printk("  FAULT CAUGHT\n");
    printk("  Reason : %u\n", reason);
    printk("  MMFAR  : 0x%08X\n", mmfar);
    printk("  CFSR   : 0x%08X\n", cfsr);

    if (esf != NULL) {
        printk("  PC     : 0x%08X\n", esf->basic.pc);
        printk("  LR     : 0x%08X\n", esf->basic.lr);
    }

    printk("========================================\n\n");
    printk("Killing thread. System continues.\n\n");

    k_thread_abort(k_current_get());
    CODE_UNREACHABLE;
}
