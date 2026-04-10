# SecuZephyr — Security Evaluation of Zephyr RTOS on STM32 Nucleo-L552ZE-Q

**Authors:** Muhammad Aetisam & Abdul Wasey
**Repository:** https://github.com/MAetisamm/SecuZephyr

---

## Overview

This project performs a hardware-based security evaluation of Zephyr RTOS
running on the STM32 Nucleo-L552ZE-Q (ARM Cortex-M33, ARMv8-M). It tests
whether built-in security mechanisms — MPU-based memory protection, user-space
isolation, syscall validation, stack guard, and ARM TF-M TrustZone — effectively
prevent user-level tasks from accessing privileged kernel resources.

Controlled attack scenarios are implemented and executed on real hardware.
System responses are observed, logged via UART, and analysed. All five MPU
experiments yielded zero breaches across 50 total runs (10 runs × 5 experiments).
Experiment 6 (TrustZone boundary crossing) confirmed SAU enforcement with
zero breaches across 10 runs.

---

## Hardware

| Parameter | Value |
|---|---|
| Board | STM32 Nucleo-L552ZE-Q (MB1361C) |
| MCU | STM32L552ZET6QU |
| Core | ARM Cortex-M33, ARMv8-M |
| Clock | 110 MHz |
| Flash | 512 KB (dual bank) |
| RAM | 256 KB |

---

## Software Requirements

| Tool | Version |
|---|---|
| Zephyr RTOS | v4.4.0-rc2 |
| Zephyr SDK | v1.0.1 |
| West | v1.5.0 |
| Compiler | arm-zephyr-eabi-gcc 14.3.0 |
| OpenOCD | v0.12.0 |
| Host OS | Ubuntu 24.04 LTS |
| STM32CubeProgrammer | v2.22.0 (option bytes only) |

---

## Build and Flash Instructions

### MPU Experiments (Experiments 1–5)

```bash
# Activate Zephyr virtual environment
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject/zephyr

# Build (replace expN_name with experiment folder)
west build -p -b nucleo_l552ze_q app/expN_name -- \
  -DCONFIG_MPU=y \
  -DCONFIG_USERSPACE=y

# Flash via OpenOCD
west flash --runner openocd

# Read UART output
minicom -D /dev/ttyACM0 -b 115200
```

### TrustZone Experiments (Experiments 6–8)

```bash
# Build with TF-M Non-Secure target
west build -p -b nucleo_l552ze_q/stm32l552xx/ns \
  app/expN_name

# Flash via OpenOCD
west flash --runner openocd

# Read UART output
minicom -D /dev/ttyACM0 -b 115200
```

> **Note:** STM32CubeProgrammer is required only for configuring TrustZone
> option bytes (TZEN, SECWM1, SECWM2, HDP1, SRAM2_RST). All code building
> and flashing uses OpenOCD on the Ubuntu host.

---

## TrustZone Option Byte Configuration

The following option bytes must be set before running Experiments 6–8.
These are set once using STM32CubeProgrammer CLI:

```
TZEN          = 0x1    (TrustZone enabled)
SECWM1_PSTRT  = 0x00   (Secure Bank 1 start: 0x08000000)
SECWM1_PEND   = 0x7F   (Secure Bank 1 end:   0x0803F800)
SECWM2_PSTRT  = 0x00   (Secure Bank 2 start: 0x08040000)
SECWM2_PEND   = 0x01   (Secure Bank 2 end:   0x08040800)
HDP1_PEND     = 0x1A   (Hide protect BL2: pages 0–26)
SRAM2_RST     = 0x0    (SRAM2 erased at reset)
RDP           = 0xAA   (Level 0, development mode)
```

---

## TF-M Flash Memory Layout

| Region | Start | End | Size |
|---|---|---|---|
| BL2 (MCUBoot) | 0x0C000000 | 0x0C00D7FF | 54 KB |
| OTP / NV counters | 0x0C00E000 | 0x0C00FFFF | 8 KB |
| Secure Storage (PS) | 0x0C010000 | 0x0C011FFF | 8 KB |
| Internal TS (ITS) | 0x0C012000 | 0x0C013FFF | 8 KB |
| TF-M Secure image | 0x0C014000 | 0x0C040FFF | 180 KB |
| Zephyr NS image | 0x08041000 | 0x08049FFF | 36 KB |
| **Secure RAM (TF-M)** | **0x30000000** | **0x3001FFFF** | **128 KB** |
| Non-Secure RAM | 0x20000000 | 0x2001FFFF | 128 KB |

---

## Experiments

| # | Name | Type | Status | Result |
|---|---|---|---|---|
| 1 | Direct Kernel Memory Access | MPU | ✅ Complete | BLOCKED 10/10 |
| 2 | Privilege Escalation via CONTROL Register | MPU | ✅ Complete | BLOCKED 10/10 |
| 3 | Syscall Abuse with Invalid Pointers | MPU | ✅ Complete | BLOCKED 10/10 |
| 4 | Stack Overflow & MPU Stack Guard | MPU | ✅ Complete | BLOCKED 10/10 |
| 5 | Cross-Thread Memory Access | MPU | ✅ Complete | BLOCKED 10/10 |
| 6 | NS Read of Secure RAM (TrustZone) | TF-M | ✅ Complete | BLOCKED 10/10 |
| 7 | NS Direct Jump to Secure Function | TF-M | 🔄 In Progress | — |
| 8 | NS Write to Secure Peripheral | TF-M | 🔄 In Progress | — |

---

## Key Findings

- **Zero breaches** across all completed experiments (60 total runs)
- **ARMv8-M architectural finding:** Unprivileged writes to ARM CONTROL register
  are silently ignored (unlike ARMv7-M which raises UsageFault) — stronger
  implicit protection with no energy side channel
- **Defence-in-depth:** Both syscall validator and MPU independently block
  Experiment 3 — redundant layers confirmed working
- **TrustZone boundary enforced:** SecureFault (escalated to HardFault) on
  every Non-Secure read of Secure RAM — SAU working correctly on real silicon

---

## Board Recovery (TrustZone Lock)

If the board becomes locked after enabling TZEN without valid NS code:

### Hardware Setup
```
1. Connect CN1  (USB Micro-B) → PC   [ST-LINK]
2. Connect CN15 (USB Type-C)  → PC   [MCU independent power]
3. Bridge CN11 pin 5 (VDD) to pin 7 (BOOT0)
4. Power cycle CN1
```

### Recovery Commands (STM32CubeProgrammer — option bytes only)
```bash
# Disable TZEN via HotPlug (CN15 + BOOT0 trick required)
STM32_Programmer_CLI.exe -c port=SWD mode=HotPlug \
  -ob TZEN=0 RDP=0xAA

# Clear write protection
STM32_Programmer_CLI.exe -c port=SWD mode=UR \
  -ob WRP1A_PSTRT=0x7F WRP1A_PEND=0x0
```

### Verify Recovery (Ubuntu host)
```bash
west build -p -b nucleo_l552ze_q samples/hello_world
west flash --runner openocd
minicom -D /dev/ttyACM0 -b 115200
# Expected: Hello World! nucleo_l552ze_q/stm32l552xx
```

---

## Project Structure

```
app/
├── exp1_kernel_access/     # Experiment 1: Direct kernel memory access
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── src/
│       ├── main.c
│       └── fault_handler.c
├── exp2_control_reg/       # Experiment 2: CONTROL register write
├── exp3_syscall_abuse/     # Experiment 3: Syscall with invalid pointer
├── exp4_stack_overflow/    # Experiment 4: Stack overflow + stack guard
├── exp5_cross_thread/      # Experiment 5: Cross-thread memory access
├── exp6_tz_ns_read/        # Experiment 6: NS read of Secure RAM
├── exp7_tz_ns_jump/        # Experiment 7: NS jump to Secure fn (WIP)
└── exp8_tz_ns_periph/      # Experiment 8: NS write to Secure periph (WIP)
```

---

## References

1. Zephyr Project. Security Overview. https://docs.zephyrproject.org
2. ARM Ltd. TF-M: Trusted Firmware-M. https://trustedfirmware.org/projects/tf-m
3. STMicroelectronics. STM32L5 Reference Manual RM0438, Rev 7, 2022.
4. ARM Ltd. ARMv8-M MPU Technical Reference Manual, ARM DDI 0553B, 2017.
5. STMicroelectronics. AN5347: STM32L5 TrustZone Features, 2021.