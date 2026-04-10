cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(exp5_cross_thread)

target_sources(app PRIVATE
    src/main.c
    src/fault_handler.c
)
