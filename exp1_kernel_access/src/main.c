#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE   1024
#define THREAD_PRIO  5

K_THREAD_STACK_DEFINE(attack_stack, STACK_SIZE);
struct k_thread attack_thread_data;

static volatile uint32_t kernel_secret = 0xDEADBEEF;

void attack_thread_entry(void *p1, void *p2, void *p3)
{
    printk("[ATTACK] Thread started in USER mode\n");
    printk("[ATTACK] Attempting illegal read...\n");

    volatile uint32_t *bad_ptr = &kernel_secret;
    uint32_t stolen = *bad_ptr;   /* MPU fires HERE */

    printk("[ATTACK] !! BREACH !! value: 0x%08X\n", stolen);
}

int main(void)
{
    printk("BOOT OK\n");
    printk("Experiment 1: Kernel Memory Access\n");
    printk("kernel_secret @ 0x%08X = 0x%08X\n",
           (uint32_t)(uintptr_t)&kernel_secret,
           kernel_secret);

    /* Grant attack thread access to its own stack only */
    k_thread_access_grant(&attack_thread_data,
                          &attack_stack);

    k_tid_t tid = k_thread_create(
        &attack_thread_data,
        attack_stack,
        K_THREAD_STACK_SIZEOF(attack_stack),
        attack_thread_entry,
        NULL, NULL, NULL,
        THREAD_PRIO,
        K_USER | K_INHERIT_PERMS,
        K_NO_WAIT
    );

    printk("[MAIN] Attack thread spawned: %p\n", (void *)tid);
    printk("[MAIN] Waiting...\n");

    k_sleep(K_SECONDS(2));

    printk("[MAIN] Supervisor still alive.\n");
    printk("[MAIN] Experiment 1 COMPLETE.\n");

    return 0;
}
