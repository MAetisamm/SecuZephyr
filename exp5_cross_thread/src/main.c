#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE     2048
#define THREAD_PRIO_A  6
#define THREAD_PRIO_B  5

K_THREAD_STACK_DEFINE(thread_a_stack, STACK_SIZE);
struct k_thread thread_a_data;

K_THREAD_STACK_DEFINE(thread_b_stack, STACK_SIZE);
struct k_thread thread_b_data;

// Thread B — victim
void thread_b_entry(void *p1, void *p2, void *p3)
{
    volatile uint32_t b_secret = 0xBEEFCAFE;

    printk("[THREAD B] Started in USER mode\n");
    printk("[THREAD B] Secret value : 0x%08X\n", b_secret);
    printk("[THREAD B] Secret addr  : 0x%08X\n",
           (uint32_t)(uintptr_t)&b_secret);

    while (1) {
        k_sleep(K_MSEC(1000));
        printk("[THREAD B] Still alive. Secret: 0x%08X\n", b_secret);
    }
}

// Thread A — attacker
void thread_a_entry(void *p1, void *p2, void *p3)
{
    /* Thread B stack base passed as argument from main */
    uintptr_t target_addr = (uintptr_t)p1;

    printk("[THREAD A] Started in USER mode\n");
    printk("[THREAD A] Targeting Thread B stack base: 0x%08X\n",
           (uint32_t)target_addr);
    printk("[THREAD A] Attempting cross-thread read...\n");

    // THE ATTACK:
    volatile uint32_t *target = (volatile uint32_t *)target_addr;
    uint32_t stolen = *target;   /* MPU fires HERE */

    // Should NEVER reach here
    printk("[THREAD A] !! BREACH !! Read: 0x%08X\n", stolen);
}

int main(void)
{
    printk("=== BOOT OK ===\n");
    printk("========================================\n");
    printk("  Experiment 5: Cross-Thread Access    \n");
    printk("  Goal: Thread A cannot read Thread B  \n");
    printk("========================================\n\n");

    k_thread_access_grant(&thread_b_data, &thread_b_stack);
    k_thread_access_grant(&thread_a_data, &thread_a_stack);

    k_thread_create(
        &thread_b_data,
        thread_b_stack,
        K_THREAD_STACK_SIZEOF(thread_b_stack),
        thread_b_entry,
        NULL, NULL, NULL,
        THREAD_PRIO_B,
        K_USER | K_INHERIT_PERMS,
        K_NO_WAIT
    );

    uintptr_t b_stack_base = (uintptr_t)thread_b_stack;

    k_thread_create(
        &thread_a_data,
        thread_a_stack,
        K_THREAD_STACK_SIZEOF(thread_a_stack),
        thread_a_entry,
        (void *)b_stack_base,   /* pass Thread B stack address */
        NULL, NULL,
        THREAD_PRIO_A,
        K_USER | K_INHERIT_PERMS,
        K_NO_WAIT
    );

    printk("[MAIN] Both threads spawned\n");
    printk("[MAIN] Thread B stack base: 0x%08X\n",
           (uint32_t)b_stack_base);
    printk("[MAIN] Waiting...\n\n");

    k_sleep(K_SECONDS(5));

    printk("[MAIN] Supervisor still alive.\n");
    printk("[MAIN] System is stable.\n");
    printk("[MAIN] Experiment 5 COMPLETE.\n");

    return 0;
}
