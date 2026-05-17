#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE   1024
#define THREAD_PRIO  5

K_THREAD_STACK_DEFINE(attack_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(work_stack,   STACK_SIZE);

struct k_thread attack_thread_data;
struct k_thread work_thread_data;

void attack_thread_entry(void *p1, void *p2, void *p3)
{
    printk("[ATTACK] Thread started in USER mode\n");

    uint32_t control_before;
    // Reading the value of control register
    __asm__ volatile ("MRS %0, CONTROL" : "=r"(control_before));
    printk("[ATTACK] CONTROL before: 0x%08X\n", control_before);
    printk("[ATTACK] nPRIV bit: %d (1=unprivileged, 0=privileged)\n",
           (control_before & 0x1));

    printk("[ATTACK] Attempting to write CONTROL register...\n");

    __asm__ volatile ("MSR CONTROL, %0" :: "r"(0));

    // Should NEVER reach here
    uint32_t control_after;
    __asm__ volatile ("MRS %0, CONTROL" : "=r"(control_after));
    printk("[ATTACK] CONTROL after  : 0x%08X\n", control_after);

    if ((control_after & 0x1) == 0) {
        printk("[ATTACK] !! BREACH !! nPRIV=0 — privilege escalation SUCCEEDED!\n");
    } else {
        printk("[ATTACK] PROTECTED — nPRIV still=1, write was ignored by hardware\n");
        printk("[ATTACK] Privilege escalation FAILED as expected\n");
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
    printk("  Experiment 2: CONTROL Register Write\n");
    printk("  Goal: Privilege escalation attempt\n");
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

    k_sleep(K_SECONDS(2));

    printk("[MAIN] Supervisor still alive.\n");
    printk("[MAIN] System is stable.\n");
    printk("[MAIN] Experiment 2 COMPLETE.\n");

    return 0;
}
