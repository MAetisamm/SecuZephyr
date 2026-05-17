#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE   2048
#define THREAD_PRIO  5

K_THREAD_STACK_DEFINE(attack_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(work_stack,   STACK_SIZE);

struct k_thread attack_thread_data;
struct k_thread work_thread_data;

static int depth = 0;

void recurse(void)
{
    volatile uint8_t buf[128];
    buf[0] = 0xAA;

    depth++;
    printk("[ATTACK] Depth: %d\n", depth);

    recurse();
}

void attack_thread_entry(void *p1, void *p2, void *p3)
{
    printk("[ATTACK] Thread started in USER mode\n");
    printk("[ATTACK] Stack size: %d bytes\n", STACK_SIZE);
    printk("[ATTACK] Starting recursive overflow...\n\n");

    recurse();

    /* Should NEVER reach here */
    printk("[ATTACK] !! BREACH !! Stack overflow not caught!\n");
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
    printk("  Experiment 4: Stack Overflow          \n");
    printk("  Goal: MPU Stack Guard catches overflow\n");
    printk("========================================\n\n");

    k_thread_access_grant(&attack_thread_data, &attack_stack);
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

    k_sleep(K_SECONDS(5));

    printk("[MAIN] Supervisor still alive.\n");
    printk("[MAIN] System is stable.\n");
    printk("[MAIN] Experiment 4 COMPLETE.\n");

    return 0;
}
