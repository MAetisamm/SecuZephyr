#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/*
 * Experiment 8 -- Non-Secure Write to Secure Peripheral
 *
 * Goal: Verify that Non-Secure code cannot write to
 * peripherals assigned to the Secure world by GTZC
 * (Global TrustZone Controller) on STM32L552.
 *
 * TF-M assigns the RNG peripheral to the Secure world.
 * RNG Secure alias base address: 0x520C0000
 *
 * Any Non-Secure write to a Secure peripheral raises
 * a SecureFault via the SAU/IDAU before the peripheral
 * register is modified.
 *
 * Three sub-attacks are tested:
 *   A) Write to Secure RNG control register
 *   B) Read from Secure RNG output register
 *   C) Write to Secure AES peripheral
 */

/* RNG peripheral Secure alias addresses */
#define RNG_S_BASE    0x520C0000UL
#define RNG_CR_OFFSET 0x00UL   /* Control register */
#define RNG_SR_OFFSET 0x04UL   /* Status register  */
#define RNG_DR_OFFSET 0x08UL   /* Data register    */

/* AES peripheral Secure alias */
#define AES_S_BASE    0x520C0400UL
#define AES_CR_OFFSET 0x00UL

void attack_rng_write(void)
{
    printk("[ATTACK-A] Writing to Secure RNG @ 0x%08X\n",
           (uint32_t)(RNG_S_BASE + RNG_CR_OFFSET));

    volatile uint32_t *rng_cr =
        (volatile uint32_t *)(RNG_S_BASE + RNG_CR_OFFSET);

    *rng_cr = 0x00000004;   /* SecureFault fires HERE */

    /* Should NEVER reach here */
    printk("[ATTACK-A] !! BREACH !! RNG write succeeded!\n");
}

void attack_rng_read(void)
{
    printk("[ATTACK-B] Reading from Secure RNG @ 0x%08X\n",
           (uint32_t)(RNG_S_BASE + RNG_DR_OFFSET));

    volatile uint32_t *rng_dr =
        (volatile uint32_t *)(RNG_S_BASE + RNG_DR_OFFSET);

    uint32_t stolen = *rng_dr;  /* SecureFault fires HERE */

    /* Should NEVER reach here */
    printk("[ATTACK-B] !! BREACH !! RNG read: 0x%08X\n",
           stolen);
}

void attack_aes_write(void)
{
    printk("[ATTACK-C] Writing to Secure AES @ 0x%08X\n",
           (uint32_t)(AES_S_BASE + AES_CR_OFFSET));

    volatile uint32_t *aes_cr =
        (volatile uint32_t *)(AES_S_BASE + AES_CR_OFFSET);

    *aes_cr = 0x00000001;   /* SecureFault fires HERE */

    /* Should NEVER reach here */
    printk("[ATTACK-C] !! BREACH !! AES write succeeded!\n");
}

void attack_thread_entry(void *p1, void *p2, void *p3)
{
    printk("[ATTACK] Thread started in Non-Secure mode\n\n");

    /* Sub-attack A: Write to Secure RNG */
    printk("--- Sub-attack A: RNG Control Register Write ---\n");
    attack_rng_write();

    /* If A is blocked, try B */
    printk("--- Sub-attack B: RNG Data Register Read ---\n");
    attack_rng_read();

    /* If B is blocked, try C */
    printk("--- Sub-attack C: AES Control Register Write ---\n");
    attack_aes_write();

    printk("[ATTACK] All sub-attacks blocked.\n");
}

#define STACK_SIZE  2048
#define PRIO        5

K_THREAD_STACK_DEFINE(attack_stack, STACK_SIZE);
struct k_thread attack_thread;

int main(void)
{
    printk("=== BOOT OK (Non-Secure World) ===\n");
    printk("==========================================\n");
    printk("  Experiment 8: NS Write Secure Periph   \n");
    printk("  Board: nucleo_l552ze_q (TF-M active)   \n");
    printk("  Attack A: RNG CR write @ 0x520C0000    \n");
    printk("  Attack B: RNG DR read  @ 0x520C0008    \n");
    printk("  Attack C: AES CR write @ 0x520C0400    \n");
    printk("  Expected: SecureFault from SAU/GTZC    \n");
    printk("==========================================\n\n");

    k_thread_create(
        &attack_thread,
        attack_stack,
        K_THREAD_STACK_SIZEOF(attack_stack),
        attack_thread_entry,
        NULL, NULL, NULL,
        PRIO, 0, K_NO_WAIT
    );

    k_sleep(K_SECONDS(5));

    printk("[MAIN] Non-Secure world still running.\n");
    printk("[MAIN] Experiment 8 COMPLETE.\n");

    return 0;
}
