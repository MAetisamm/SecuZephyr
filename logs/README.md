# UART Logs — SecuZephyr Security Experiments

Raw UART output captured from the STM32 Nucleo-L552ZE-Q board
via minicom at 115200 baud on Ubuntu 24.04 LTS.

## Capture Method
minicom -D /dev/ttyACM0 -b 115200 -C logfile.txt

## Board
NUCLEO-L552ZE-Q, STM32L552ZET6QU, ARM Cortex-M33
Zephyr RTOS v4.4.0-rc2, TF-M v2.2.2

## Experiment Results Summary
| Exp | Runs | Result       |
|-----|------|--------------|
| 1   | 10   | BLOCKED 10/10|
| 2   | 10   | BLOCKED 10/10|
| 3   | 10   | BLOCKED 10/10|
| 4   | 10   | BLOCKED 10/10|
| 5   | 10   | BLOCKED 10/10|
| 6   | 10   | BLOCKED 10/10|
| 7   | 10   | BLOCKED 10/10|
| 8   | 10   | BLOCKED 10/10|
| 9   | 3003 | VULNERABLE 7/3003 (0.23%) |

## Exp 9 TOCTOU Sessions
- session_1.txt: 1001 races, 1 attack win  (0.10%)
- session_2.txt: 1001 races, 2 attack wins (0.20%)
- session_3.txt: 1001 races, 4 attack wins (0.40%)
