#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE   1024
#define THREAD_PRIO  5

/* Message queue setup */
#define MSG_SIZE  4
#define MSG_COUNT 4

K_MSGQ_DEFINE(test_msgq, MSG_SIZE, MSG_COUNT, 4);

K_THREAD_STACK_DEFINE(attack_stack, STACK_SIZE);
struct k_thread attack_thread_data;

/* Kernel variable — attacker wants to pass this as buffer */
static volatile uint32_t kernel_secret = 0xDEADBEEF;

void attack_thread_entry(void *p1, void *p2, void *p3)
{
    printk("[ATTACK] Thread started in USER mode\n");
    printk("[ATTACK] kernel_secret @ 0x%08X = 0x%08X\n",
           (uint32_t)(uintptr_t)&kernel_secret,
           kernel_secret);

    printk("[ATTACK] Calling k_msgq_put() with kernel pointer as buffer...\n");

    int ret = k_msgq_put(&test_msgq,
                         (void *)&kernel_secret,  /* invalid: kernel pointer */
                         K_NO_WAIT);

    printk("[ATTACK] k_msgq_put() returned: %d\n", ret);

    if (ret == 0) {
        printk("[ATTACK] !! BREACH !! Syscall accepted kernel pointer!\n");
        printk("[ATTACK] Kernel memory may have been exposed!\n");
    } else {
        printk("[ATTACK] PROTECTED — syscall rejected kernel pointer\n");
        printk("[ATTACK] Return code %d means access was denied\n", ret);
    }
}

int main(void)
{
    printk("=== BOOT OK ===\n");
    printk("========================================\n");
    printk("  Experiment 3: Syscall Abuse          \n");
    printk("  Goal: Pass kernel pointer to syscall \n");
    printk("========================================\n\n");

    k_thread_access_grant(&attack_thread_data,
                          &attack_stack,
                          &test_msgq);

    k_thread_create(
        &attack_thread_data,
        attack_stack,
        K_THREAD_STACK_SIZEOF(attack_stack),
        attack_thread_entry,
        NULL, NULL, NULL,
        THREAD_PRIO,
        K_USER | K_INHERIT_PERMS,
        K_NO_WAIT
    );

    printk("[MAIN] Attack thread spawned\n");
    printk("[MAIN] Waiting...\n\n");

    k_sleep(K_SECONDS(2));

    printk("[MAIN] Supervisor still alive.\n");
    printk("[MAIN] System is stable.\n");
    printk("[MAIN] Experiment 3 COMPLETE.\n");

    return 0;
}
