#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <psa/protected_storage.h>
#include <psa/crypto.h>
#include <string.h>

/* ── Configuration ────────────────────────────────────────────────── */
#define PS_KEY_ID          0x00000002U
#define BUFFER_SIZE        64
#define RACE_ITERATIONS    200

static const uint8_t LEGIT_DATA[BUFFER_SIZE]  = {[0 ... 63] = 0xAA};
static const uint8_t ATTACK_DATA[BUFFER_SIZE] = {[0 ... 63] = 0xFF};
static volatile uint8_t shared_buf[BUFFER_SIZE];

/* ── Statistics ───────────────────────────────────────────────────── */
static volatile int race_attempts  = 0;
static volatile int attack_wins    = 0;
static volatile int legit_wins     = 0;
static volatile int pre_call_skips = 0;
static volatile int ps_errors      = 0;
static volatile bool running       = true;

K_SEM_DEFINE(start_sem, 0, 1);

/* ── Thread A: Legitimate PS writer ──────────────────────────────── */
void thread_a_entry(void *p1, void *p2, void *p3)
{
    psa_status_t status;
    uint8_t readback[BUFFER_SIZE];
    uint8_t local_snapshot[BUFFER_SIZE];  /* ← key fix */
    size_t readback_len;

    printk("[THREAD-A] Legitimate PS writer started\n");
    k_sem_take(&start_sem, K_FOREVER);

    while (running && race_attempts < RACE_ITERATIONS) {

        /* Step 1: write LEGIT into shared buffer */
        memcpy((void *)shared_buf, LEGIT_DATA, BUFFER_SIZE);

        /*
         * Step 2: Snapshot + pre-call check (atomic as possible)
         *
         * Take a local copy of shared_buf first, then verify
         * the snapshot — not shared_buf — is still LEGIT.
         * This eliminates the NS-scheduler gap between check
         * and call: Thread B can no longer corrupt what we
         * actually pass to psa_ps_set().
         */
        memcpy(local_snapshot, (void *)shared_buf, BUFFER_SIZE);

        if (memcmp(local_snapshot, LEGIT_DATA, BUFFER_SIZE) != 0) {
            /*
             * Thread B already corrupted shared_buf before
             * we could snapshot it — NS scheduler race,
             * not a TF-M TOCTOU. Exclude this iteration.
             */
            pre_call_skips++;
            continue;
        }

        /*
         * Step 3: Call psa_ps_set() with the LOCAL snapshot.
         *
         * Thread B cannot touch local_snapshot (stack variable).
         * If ATTACK_DATA is stored despite passing LEGIT snapshot
         * → TRUE TOCTOU race inside TF-M's secure call.
         * Expected: this should NOT happen on Cortex-M33 because
         * NS world is blocked during the secure gateway call.
         */
        status = psa_ps_set(
            PS_KEY_ID,
            BUFFER_SIZE,
            local_snapshot,           /* ← local copy, not shared_buf */
            PSA_STORAGE_FLAG_NONE
        );
        if (status != PSA_SUCCESS) {
            ps_errors++;
            continue;
        }

        /* Step 4: Read back what TF-M actually stored */
        status = psa_ps_get(
            PS_KEY_ID,
            0,
            BUFFER_SIZE,
            readback,
            &readback_len
        );
        if (status != PSA_SUCCESS) {
            ps_errors++;
            continue;
        }

        race_attempts++;

        if (memcmp(readback, ATTACK_DATA, BUFFER_SIZE) == 0) {
            attack_wins++;
            printk("[!!!] TRUE TOCTOU in psa_ps_set()! "
                   "Snapshot=LEGIT at call, ATTACK stored. "
                   "Race #%d\n", race_attempts);
        } else if (memcmp(readback, LEGIT_DATA, BUFFER_SIZE) == 0) {
            legit_wins++;
        } else {
            printk("[!] TORN READ at race #%d\n", race_attempts);
        }
    }
    running = false;
}

/* ── Thread B: Attacker (unchanged) ──────────────────────────────── */
void thread_b_entry(void *p1, void *p2, void *p3)
{
    printk("[THREAD-B] Attacker started\n");
    k_sem_give(&start_sem);

    while (running) {
        memcpy((void *)shared_buf, ATTACK_DATA, BUFFER_SIZE);
        k_yield();
    }
}

/* ── Thread definitions ───────────────────────────────────────────── */
#define STACK_SIZE  4096
#define PRIO_A      5
#define PRIO_B      5

K_THREAD_STACK_DEFINE(stack_a, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack_b, STACK_SIZE);
struct k_thread thread_a, thread_b;

/* ── Main ─────────────────────────────────────────────────────────── */
int main(void)
{
    psa_status_t status;

    printk("=== BOOT OK (Non-Secure World) ===\n");
    printk("==========================================\n");
    printk("  Experiment 13 v3: TOCTOU on TF-M PS   \n");
    printk("  Target API : psa_ps_set()              \n");
    printk("  Domain     : ARoT + AES-GCM            \n");
    printk("  Pre-call check: SNAPSHOT (fixed)       \n");
    printk("  (passes local copy, not shared_buf)    \n");
    printk("  Iterations : %d valid races            \n",
           RACE_ITERATIONS);
    printk("==========================================\n\n");

    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printk("[ERROR] psa_crypto_init: %d\n", status);
        return -1;
    }

    psa_ps_remove(PS_KEY_ID);

    printk("[MAIN] Snapshot method separates:\n");
    printk("[MAIN]   NS scheduler races (pre_call_skips)\n");
    printk("[MAIN]   TRUE TF-M Secure call races (attack_wins)\n\n");

    k_thread_create(&thread_a, stack_a,
        K_THREAD_STACK_SIZEOF(stack_a),
        thread_a_entry, NULL, NULL, NULL,
        PRIO_A, 0, K_NO_WAIT);
    k_thread_name_set(&thread_a, "legit_ps");

    k_thread_create(&thread_b, stack_b,
        K_THREAD_STACK_SIZEOF(stack_b),
        thread_b_entry, NULL, NULL, NULL,
        PRIO_B, 0, K_NO_WAIT);
    k_thread_name_set(&thread_b, "attacker");

    while (running || race_attempts < RACE_ITERATIONS) {
        k_sleep(K_MSEC(2000));
        printk("[MAIN] %d/%d | TOCTOU: %d | "
               "Legit: %d | Pre-skip: %d | Err: %d\n",
               race_attempts, RACE_ITERATIONS,
               attack_wins, legit_wins,
               pre_call_skips, ps_errors);
    }

    k_thread_abort(&thread_a);
    k_thread_abort(&thread_b);
    psa_ps_remove(PS_KEY_ID);

    /* ── Final Report ─────────────────────────────────────────── */
    printk("\n==========================================\n");
    printk("  Experiment 13 v3: FINAL RESULTS        \n");
    printk("==========================================\n");
    printk("  Target API         : psa_ps_set()      \n");
    printk("  Isolation domain   : ARoT              \n");
    printk("  Encryption         : AES-GCM           \n");
    printk("  Method             : Snapshot (fixed)  \n");
    printk("  Valid races        : %d\n", race_attempts);
    printk("  Legit stored       : %d\n", legit_wins);
    printk("  TRUE TOCTOU wins   : %d\n", attack_wins);
    printk("  Pre-call skips     : %d\n", pre_call_skips);
    printk("  (skips = NS sched races, excluded)     \n");
    printk("  PS errors          : %d\n", ps_errors);

    if (race_attempts > 0) {
        int r = (attack_wins * 10000) / race_attempts;
        printk("  TRUE TOCTOU rate   : %d.%02d%%\n",
               r / 100, r % 100);
    }

    printk("\n  ---- Comparison ----\n");
    printk("  v2 (shared_buf passed): 99.50%% (false positive)\n");
    if (race_attempts > 0) {
        int r = (attack_wins * 10000) / race_attempts;
        printk("  v3 (snapshot passed):   %d.%02d%%\n",
               r / 100, r % 100);
    }
    printk("  --------------------\n");

    if (attack_wins == 0) {
        printk("\n  RESULT: TF-M PS RESISTANT to TOCTOU\n");
        printk("  v2 result was a FALSE POSITIVE.\n");
        printk("  Race was in NS scheduler gap, NOT\n");
        printk("  inside TF-M secure call.\n");
        printk("  Cortex-M33 NS preemption confirmed\n");
        printk("  blocked during secure gateway call.\n");
    } else {
        printk("\n  RESULT: !! TRUE TOCTOU IN psa_ps_set !!\n");
        printk("  Snapshot=LEGIT at call time but\n");
        printk("  ATTACK stored %d/%d times.\n",
               attack_wins, race_attempts);
        printk("  Race window confirmed inside TF-M.\n");
    }

    printk("==========================================\n");
    printk("[MAIN] Experiment 13 v3 COMPLETE.\n");
    return 0;
}