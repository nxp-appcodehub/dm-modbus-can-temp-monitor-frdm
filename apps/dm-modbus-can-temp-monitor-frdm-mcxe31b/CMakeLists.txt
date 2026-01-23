# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)


find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(CAN)

zephyr_compile_definitions(CONFIG_CAN_MAX_MB=96)

# FLEXCAN_0 -> CONFIG_CAN_MAX_MB=96
# FLEXCAN_1 -> CONFIG_CAN_MAX_MB=64
# FLEXCAN_2 -> CONFIG_CAN_MAX_MB=64
# FLEXCAN_3 -> CONFIG_CAN_MAX_MB=32
# FLEXCAN_4 -> CONFIG_CAN_MAX_MB=32
# FLEXCAN_5 -> CONFIG_CAN_MAX_MB=32

target_sources(app PRIVATE src/main.c)
