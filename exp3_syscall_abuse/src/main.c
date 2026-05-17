#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE   1024
#define THREAD_PRIO  5

/* Message queue setup */
#define MSG_SIZE  4
#define MSG_COUNT 4

K_MSGQ_DEFINE(test_msgq, MSG_SIZE, MSG_COUNT, 4);

K_THREAD_STACK_DEFINE(attack_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(work_stack,   STACK_SIZE);

struct k_thread attack_thread_data;
struct k_thread work_thread_data;


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
    printk("=== BOOT OK ===\n");
    printk("========================================\n");
    printk("  Experiment 3: Syscall Abuse          \n");
    printk("  Goal: Pass kernel pointer to syscall \n");
    printk("========================================\n\n");

    k_thread_access_grant(&attack_thread_data,
                          &attack_stack,
                          &test_msgq);

    k_thread_access_grant(&work_thread_data,
                          &work_stack);
    k_tid_t tid1 = k_thread_create(
        &work_thread_data,
        work_stack,
        K_THREAD_STACK_SIZEOF(work_stack),
        do_work,
        NULL, NULL, NULL,
        THREAD_PRIO,
        K_USER | K_INHERIT_PERMS,
        K_NO_WAIT
    );
    printk("[MAIN] Work thread spawned: %p\n", (void *)tid1);
    printk("[MAIN] Work in Progress\n");
    k_sleep(K_SECONDS(10));


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
