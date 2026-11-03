# SecuZephyr — Security Evaluation of Zephyr RTOS on STM32 Nucleo-L552ZE-Q

**Author:** Muhammad Aetisam
**Repository:** https://github.com/MAetisamm/SecuZephyr

---

## Overview

This project presents a comprehensive hardware-based security evaluation of
Zephyr RTOS v4.4.0-rc2 and TF-M v2.2.2 running on the STM32 Nucleo-L552ZE-Q
(ARM Cortex-M33, ARMv8-M). Nine controlled attack scenarios are implemented
and executed on real hardware across three security layers:

- **Experiments 1–5:** MPU-based memory isolation and user-space protection
- **Experiments 6–8:** ARM TrustZone-M boundary enforcement via TF-M
- **Experiment 9:** TOCTOU race condition attack on the PSA Internal Trusted Storage (ITS) API

All experiments were executed on physical hardware with UART logs captured for each run.

### 🚨 Key Finding — TOCTOU Vulnerability in TF-M v2.2.2

Experiment 9 reveals a TOCTOU vulnerability in TF-M v2.2.2's
`psa_its_set()` API. Attacker-controlled data was successfully stored in Secure ITS
in **19 of 10,010 race attempts** across 10 independent hardware runs (mean success
rate: **0.19%**, manifesting in **8 of 10 sessions**).

The vulnerability exists because TF-M's default PSA Level 1 configuration does **not**
implement the internal transient buffer that protects Profile Small against TOCTOU.
The attack requires no privilege escalation and operates entirely within Non-Secure
world privileges using two cooperative Zephyr threads.

> **Disclosure:** This finding will be reported to the TF-M security team at
> https://trustedfirmware.org/security/.

---

## Hardware

| Parameter | Value |
|---|---|
| Board | STM32 Nucleo-L552ZE-Q (MB1361C, Rev Z) |
| MCU | STM32L552ZET6QU |
| Core | ARM Cortex-M33, ARMv8-M |
| Clock | 110 MHz |
| Flash | 512 KB (dual bank) |
| RAM | 256 KB |

---

## Software Requirements

| Tool | Version | Purpose |
|---|---|---|
| Zephyr RTOS | v4.4.0-rc2 | RTOS |
| TF-M | v2.2.2 | Trusted Firmware |
| Zephyr SDK | v1.0.1 | Compiler toolchain |
| West | v1.5.0 | Build system |
| Compiler | arm-zephyr-eabi-gcc 14.3.0 | Cross compiler |
| OpenOCD | v0.12.0 | Flashing (all experiments) |
| Host OS | Ubuntu 24.04 LTS | Development host |
| STM32CubeProgrammer | v2.22.0 | **Option bytes only** (not for flashing) |

> **Important:** All code building and flashing is done via OpenOCD on Ubuntu.
> STM32CubeProgrammer is used exclusively for TrustZone option bytes (TZEN, SECWM, HDP1).

---

## Experiment Results Summary

| # | Experiment | Type | Runs | Result |
|---|---|---|---|---|
| 1 | Direct Kernel Memory Access | MPU | 10 | ✅ BLOCKED 10/10 |
| 2 | Privilege Escalation via CONTROL Register | MPU | 10 | ✅ BLOCKED 10/10 |
| 3 | Syscall Abuse with Invalid Pointers | MPU | 10 | ✅ BLOCKED 10/10 |
| 4 | Stack Overflow & MPU Stack Guard | MPU | 10 | ✅ BLOCKED 10/10 |
| 5 | Cross-Thread Memory Access | MPU | 10 | ✅ BLOCKED 10/10 |
| 6 | NS Read of Secure RAM | TF-M | 10 | ✅ BLOCKED 10/10 |
| 7 | NS Direct Jump to Secure Function | TF-M | 10 | ✅ BLOCKED 10/10 |
| 8 | NS Write to Secure Peripheral | TF-M | 10 | ✅ BLOCKED 10/10 |
| **9** | **TOCTOU on PSA ITS API** | **TF-M** | **10×1001** | 🚨 **VULNERABLE 0.19%** |

**Total runs:** 10,090 | **Total experiments:** 9 | **Zero breaches in Exp 1–8**

### Experiment 9 Detailed Results (10 Independent Sessions)

| Run | Races | Attack Wins | Rate | Verdict |
|---|---|---|---|---|
| 1 | 1001 | 0 | 0.00% | Resistant |
| 2 | 1001 | 1 | 0.10% | Vulnerable |
| 3 | 1001 | 4 | 0.40% | Vulnerable |
| 4 | 1001 | 0 | 0.00% | Resistant |
| 5 | 1001 | 2 | 0.20% | Vulnerable |
| 6 | 1001 | 2 | 0.20% | Vulnerable |
| 7 | 1001 | 1 | 0.10% | Vulnerable |
| 8 | 1001 | 1 | 0.10% | Vulnerable |
| 9 | 1001 | 3 | 0.30% | Vulnerable |
| 10 | 1001 | 5 | 0.50% | Vulnerable |
| **Total** | **10,010** | **19** | **0.19%** | **8/10 sessions vulnerable** |

---

## Key Findings

### 1. MPU Isolation is Effective
All five MPU attack scenarios blocked with 100% consistency (50 runs, zero breaches).
MMFAR values consistently matched attacked addresses confirming precise hardware attribution.

### 2. ARMv8-M Architectural Improvement
Experiment 2 confirms that ARMv8-M silently discards unprivileged writes to the CONTROL
register (unlike ARMv7-M which raises a UsageFault). This provides:
- No signal to attacker that the attempt was detected
- No system disruption
- No energy spike side channel

### 3. TrustZone Boundary Enforced
All three TrustZone boundary crossing experiments (Exp 6–8) produced `FATAL ERROR: HardFault`
(SecureFault escalated) on every run, confirming SAU and GTZC enforcement is working correctly
on real STM32L552 silicon.

### 4. 🚨 TOCTOU Race Condition in TF-M v2.2.2 ITS
`psa_its_set()` in TF-M v2.2.2 default configuration does not copy the NS buffer into
Secure RAM before validation. A racing NS thread can corrupt the buffer between TF-M's
pointer validation (CHECK) and the Secure copy operation (USE).

**Root cause:** TF-M Profile Small implements a transient buffer to mitigate TOCTOU,
but this is absent in the default PSA Level 1 profile used with Zephyr.

**Impact:** ~526 ITS write operations expected per successful corruption. Relevant for:
- Firmware update NV counters
- Session tokens
- Security configuration storage

**Fix:** Enable the internal transient buffer (already in Profile Small) in the default
PSA Level 1 profile.

### 5. Defence-in-Depth Confirmed
Experiment 3 shows both the syscall validator and MPU independently blocked the attack —
two separate layers functioning correctly.

---

## Build and Flash Instructions

### MPU Experiments (Experiments 1–5)

```bash
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject/zephyr

# Build (replace expN_name with experiment folder name)
west build -p -b nucleo_l552ze_q app/expN_name \
  -- -DCONFIG_MPU=y -DCONFIG_USERSPACE=y \
     -DCONFIG_MPU_STACK_GUARD=y

# Flash via OpenOCD (Ubuntu host)
west flash --runner openocd

# Capture UART log
stty -F /dev/ttyACM0 115200 raw
cat /dev/ttyACM0 | tee app/logs/expN_name/run_01.txt
```

### TrustZone + TOCTOU Experiments (Experiments 6–9)

```bash
# Build with TF-M Non-Secure target
west build -p -b nucleo_l552ze_q/stm32l552xx/ns \
  app/expN_name

# Flash via OpenOCD (Ubuntu host)
west flash --runner openocd

# Capture UART log
stty -F /dev/ttyACM0 115200 raw
cat /dev/ttyACM0 | tee app/logs/expN_name/run_01.txt
```

---

## TrustZone Option Byte Configuration

Set once using STM32CubeProgrammer CLI (option bytes only — not code flashing):

```
TZEN          = 0x1    (TrustZone globally enabled)
SECWM1_PSTRT  = 0x00   (Secure Bank 1 start: page 0  → 0x08000000)
SECWM1_PEND   = 0x7F   (Secure Bank 1 end:   page 127 → 0x0803F800)
SECWM2_PSTRT  = 0x00   (Secure Bank 2 start: page 0  → 0x08040000)
SECWM2_PEND   = 0x01   (Secure Bank 2 end:   page 1  → 0x08040800)
HDP1_PEND     = 0x1A   (Hide protect BL2: pages 0–26)
SRAM2_RST     = 0x0    (SRAM2 erased at system reset)
RDP           = 0xAA   (Level 0, open — development mode)
```

### CLI Commands Used

```bash
# Step 1: Flash TF-M merged image and enable TZEN atomically
STM32_Programmer_CLI.exe -c port=SWD mode=UR \
  -w tfm_merged.hex -ob TZEN=1 -rst

# Step 2: Set SECWM2 watermark
STM32_Programmer_CLI.exe -c port=SWD mode=HotPlug \
  -ob SECWM2_PSTRT=0x0 SECWM2_PEND=0x1

# Step 3: Set HDP1 and SRAM2_RST
STM32_Programmer_CLI.exe -c port=SWD mode=HotPlug \
  -ob HDP1_PEND=0x1A SRAM2_RST=0x0
```

---

## TF-M Flash Memory Layout

| Region | Start Address | End Address | Size |
|---|---|---|---|
| BL2 (MCUBoot) | `0x0C000000` | `0x0C00D7FF` | 54 KB |
| OTP / NV counters | `0x0C00E000` | `0x0C00FFFF` | 8 KB |
| Secure Storage (PS) | `0x0C010000` | `0x0C011FFF` | 8 KB |
| Internal TS (ITS) | `0x0C012000` | `0x0C013FFF` | 8 KB |
| TF-M Secure image | `0x0C014000` | `0x0C040FFF` | 180 KB |
| Zephyr NS image | `0x08041000` | `0x08049FFF` | 36 KB |
| **Secure RAM (TF-M)** ← Exp 6 target | **`0x30000000`** | **`0x3001FFFF`** | **128 KB** |
| Non-Secure RAM | `0x20000000` | `0x2001FFFF` | 128 KB |
| Secure RNG peripheral ← Exp 8 target | `0x520C0000` | — | — |

---

## Board Recovery (TrustZone Lockout)

If TZEN=1 is enabled without valid NS image at `0x08041000`, the board enters
a double-fault loop blocking all SWD access. Recovery using the CN15+CN11 trick:

### Hardware Setup (CN15+CN11 method — not covered in ST AN5347)
```
1. Connect CN1  (USB Micro-B) → PC   [powers ST-LINK]
2. Connect CN15 (USB Type-C)  → PC   [powers MCU independently]
3. Bridge CN11 pin 5 (VDD) to pin 7 (BOOT0)
4. Power cycle CN1
```

This places the CPU in RSS bootloader (Non-Secure state), enabling HotPlug SWD.

### Recovery Commands
```bash
# Disable TZEN (option bytes only via CubeProgrammer)
STM32_Programmer_CLI.exe -c port=SWD mode=HotPlug \
  -ob TZEN=0 RDP=0xAA

# Clear write protection
STM32_Programmer_CLI.exe -c port=SWD mode=UR \
  -ob WRP1A_PSTRT=0x7F WRP1A_PEND=0x0

# Verify recovery (Ubuntu host — OpenOCD)
west build -p -b nucleo_l552ze_q samples/hello_world
west flash --runner openocd
# Expected: Hello World! nucleo_l552ze_q/stm32l552xx
```

---

## Project Structure

```
SecuZephyr/
├── README.md
├── app/
│   ├── exp1_kernel_access/      # MPU: direct kernel memory read
│   │   ├── CMakeLists.txt
│   │   ├── prj.conf
│   │   └── src/main.c
│   ├── exp2_control_reg/        # MPU: CONTROL register privilege escalation
│   ├── exp3_syscall_abuse/      # MPU: syscall with kernel pointer
│   ├── exp4_stack_overflow/     # MPU: stack overflow + stack guard
│   ├── exp5_cross_thread/       # MPU: cross-thread memory access
│   ├── exp6_tz_ns_read/         # TF-M: NS read of Secure RAM (0x30000000)
│   ├── exp7_tz_ns_jump/         # TF-M: NS direct jump to Secure fn
│   ├── exp8_tz_ns_periph/       # TF-M: NS write to Secure RNG (0x520C0000)
│   └── exp9_toctou/             # TF-M: TOCTOU on psa_its_set()
└── logs/
    ├── exp1_kernel_access/      # 10 UART run logs
    ├── exp2_control_reg/
    ├── exp3_syscall_abuse/
    ├── exp4_stack_overflow/
    ├── exp5_cross_thread/
    ├── exp6_tz_ns_read/
    ├── exp7_tz_ns_jump/
    ├── exp8_tz_ns_periph/
    └── exp9_toctou/             # 10 sessions × 1001 races each
```

---

## UART Log Format

All logs captured on Ubuntu via:
```bash
stty -F /dev/ttyACM0 115200 raw
cat /dev/ttyACM0 | tee app/logs/expN/run_XX.txt
```

Each log file contains the full UART output of one experimental run including
TF-M boot messages, attack output, fault registers, and final result.

---

## References

1. Zephyr Project. Security Overview. https://docs.zephyrproject.org/latest/security
2. ARM Ltd. TF-M: Trusted Firmware-M v2.2.2. https://trustedfirmware.org/projects/tf-m
3. TF-M Profile Small Design (transient buffer / TOCTOU mitigation). https://trustedfirmware-m.readthedocs.io/en/latest/configuration/profiles/tfm_profile_small.html
4. STMicroelectronics. STM32L5 Reference Manual RM0438, Rev 7, 2022.
5. STMicroelectronics. AN5347: STM32L5 TrustZone Features, 2021.
6. ARM Ltd. ARMv8-M MPU Technical Reference Manual, ARM DDI 0553B, 2017.
