#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/*
 * Experiment 6 -- Non-Secure Read of Secure RAM
 *
 * Goal: Verify that Non-Secure Zephyr code cannot
 * directly read Secure RAM managed by TF-M.
 *
 * Secure RAM is aliased at 0x30000000 on STM32L5.
 * The SAU marks this region Secure.
 * Any Non-Secure access raises a SecureFault.
 */

void attack_thread_entry(void *p1, void *p2, void *p3)
{
    printk("[ATTACK] Thread started in Non-Secure mode\n");
    printk("[ATTACK] Attempting to read Secure RAM @ 0x30000000\n");
    printk("[ATTACK] This should raise a SecureFault...\n\n");

    /*
     * THE ATTACK:
     * Dereference Secure RAM alias address 0x30000000
     * Non-Secure code has NO permission to access this.
     * SAU raises SecureFault immediately.
     */
    volatile uint32_t *secure_ptr = (volatile uint32_t *)0x30000000;
    uint32_t stolen = *secure_ptr;   /* SecureFault fires HERE */

    /* Should NEVER reach here */
    printk("[ATTACK] !! BREACH !! Read value: 0x%08X\n", stolen);
    printk("[ATTACK] TrustZone boundary FAILED!\n");
}

#define STACK_SIZE  1024
#define PRIO        5

K_THREAD_STACK_DEFINE(attack_stack, STACK_SIZE);
struct k_thread attack_thread;

K_THREAD_STACK_DEFINE(work_stack,   STACK_SIZE);
struct k_thread work_thread_data;

void do_work()
{
    int i=0;
    while(true)
    {
        ++i;
        printk("[DO_WORK] Incrementing %d\n",i);
        k_sleep(K_SECONDS(1));

    }
}

int main(void)
{
    printk("=== BOOT OK (Non-Secure World) ===\n");
    printk("==========================================\n");
    printk("  Experiment 6: NS Read of Secure RAM    \n");
    printk("  Board: nucleo_l552ze_q (TF-M active)   \n");
    printk("  Attack: read from 0x30000000            \n");
    printk("  Expected: SecureFault from SAU          \n");
    printk("==========================================\n\n");
    k_thread_access_grant(&work_thread_data,
                        &work_stack);

    k_tid_t tid1 = k_thread_create(
        &work_thread_data,
        work_stack,
        K_THREAD_STACK_SIZEOF(work_stack),
        do_work,
        NULL, NULL, NULL,
        PRIO,
        K_USER | K_INHERIT_PERMS,
        K_NO_WAIT
    );
    printk("[MAIN] Work thread spawned: %p\n", (void *)tid1);
    printk("[MAIN] Work in Progress\n");
    k_sleep(K_SECONDS(10));

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
    printk("[MAIN] Experiment 6 COMPLETE.\n");

    return 0;
}
