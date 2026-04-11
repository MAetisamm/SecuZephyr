#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

typedef void (*secure_fn_t)(void);

void attack_thread_entry(void *p1, void *p2, void *p3)
{
    printk("[ATTACK] Thread started in Non-Secure mode\n");
    printk("[ATTACK] Attempting direct jump to Secure flash\n");
    printk("[ATTACK] Target: 0x0C000001 (TF-M Secure, thumb)\n");
    printk("[ATTACK] This should raise a SecureFault...\n\n");

    secure_fn_t target = (secure_fn_t)0x0C000001;
    target();   /* SecureFault fires HERE */

    /* Should NEVER reach here */
    printk("[ATTACK] !! BREACH !! Jumped into Secure world!\n");
    printk("[ATTACK] TrustZone NSC gateway BYPASSED!\n");
}

#define STACK_SIZE  1024
#define PRIO        5

K_THREAD_STACK_DEFINE(attack_stack, STACK_SIZE);
struct k_thread attack_thread;

int main(void)
{
    printk("=== BOOT OK (Non-Secure World) ===\n");
    printk("==========================================\n");
    printk("  Experiment 7: NS Jump to Secure Fn     \n");
    printk("  Board: nucleo_l552ze_q (TF-M active)   \n");
    printk("  Attack: branch to 0x0C000001            \n");
    printk("  Expected: SecureFault from SAU          \n");
    printk("==========================================\n\n");

    k_thread_create(
        &attack_thread,
        attack_stack,
        K_THREAD_STACK_SIZEOF(attack_stack),
        attack_thread_entry,
        NULL, NULL, NULL,
        PRIO, 0, K_NO_WAIT
    );

    k_sleep(K_SECONDS(3));

    printk("[MAIN] Non-Secure world still running.\n");
    printk("[MAIN] Experiment 7 COMPLETE.\n");

    return 0;
}
