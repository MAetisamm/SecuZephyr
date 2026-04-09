#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE   2048
#define THREAD_PRIO  5

K_THREAD_STACK_DEFINE(attack_stack, STACK_SIZE);
struct k_thread attack_thread_data;

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

int main(void)
{
    printk("=== BOOT OK ===\n");
    printk("========================================\n");
    printk("  Experiment 4: Stack Overflow          \n");
    printk("  Goal: MPU Stack Guard catches overflow\n");
    printk("========================================\n\n");

    k_thread_access_grant(&attack_thread_data, &attack_stack);

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
