# SecuZephyr — Security Evaluation of Zephyr RTOS on STM32 Nucleo-L552ZE-Q

## Overview
This project performs a hardware-based security evaluation of Zephyr RTOS 
running on STM32 Cortex-M33. It tests whether built-in security mechanisms 
— MPU-based memory protection, user-space isolation, and ARM TF-M TrustZone 
— effectively prevent user-level tasks from accessing privileged kernel resources.

## Hardware
- Board: STM32 Nucleo-L552ZE-Q
- MCU: STM32L552ZET6 (ARM Cortex-M33, ARMv8-M)
- Zephyr Version: v4.4.0-rc2

## Requirements
- Zephyr RTOS v4.4.0-rc2
- west build tool
- ARM Zephyr SDK
- STM32 ST-LINK driver
- OpenOCD

## Build Instructions
```bash
west build -p -b nucleo_l552ze_q app/EXPERIMENT_FOLDER -- \
  -DCONFIG_MPU=y \
  -DCONFIG_USERSPACE=y
west flash --runner openocd
```

## Experiments
| # | Name | Status |
|---|------|--------|
| 1 | Direct Kernel Memory Access | ✅ Complete |
| 2 | Privilege Escalation via CONTROL Register | ✅ Upcoming |
| 3 | Syscall Abuse with Invalid Pointers | ✅ Upcoming |
| 4 | Stack Overflow & MPU Stack Guard | 🔄 Upcoming |
| 5 | Cross-Thread Memory Access | 🔄 Upcoming |
| 6 | TrustZone Boundary Crossing (Optional) | 🔄 Upcoming |

## Results Summary
See each experiment folder for full UART logs and findings.

### Experiment 1 — Direct Kernel Memory Access
- MPU fault triggered: 10/10 runs
- Breach successful: 0/10
- System survived: 10/10
- MMFAR: 0x20000040 (consistent across all runs)
