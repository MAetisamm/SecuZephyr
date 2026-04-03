#include <zephyr/kernel.h>
#include <cmsis_core.h>

extern uint32_t saved_mmfar;
extern uint32_t saved_cfsr;

/* This runs at the very start of ARM MemManage fault */
void z_arm_fault_init(void)
{
    saved_mmfar = SCB->MMFAR;
    saved_cfsr  = SCB->CFSR;
}
