#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE   1024
#define THREAD_PRIO  5

K_THREAD_STACK_DEFINE(attack_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(work_stack,   STACK_SIZE);

struct k_thread attack_thread_data;
struct k_thread work_thread_data;

static volatile uint32_t kernel_secret = 0xDEADBEEF;

void attack_thread_entry(void *p1, void *p2, void *p3)
{
    printk("[ATTACK] Thread started in USER mode\n");
    printk("[ATTACK] Attempting illegal read...\n");

    volatile uint32_t *bad_ptr = &kernel_secret;
    uint32_t stolen = *bad_ptr;   /* MPU fires HERE */

    printk("[ATTACK] !! BREACH !! value: 0x%08X\n", stolen);
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
    printk("BOOT OK\n");
    printk("Experiment 1: Kernel Memory Access\n");
    printk("kernel_secret @ 0x%08X = 0x%08X\n",
           (uint32_t)(uintptr_t)&kernel_secret,
           kernel_secret);

    k_thread_access_grant(&work_thread_data,
                          &work_stack);

    
    /* Grant attack thread access to its own stack only */
    k_thread_access_grant(&attack_thread_data,
                          &attack_stack);


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
    printk("[MAIN] Work in Progress");
    k_sleep(K_SECONDS(10));

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
