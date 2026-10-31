# Copyright (c) 2026 Romain Pelletant
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
list(APPEND EXTRA_ZEPHYR_MODULES ${CMAKE_CURRENT_LIST_DIR}/../..)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zndef_features)
target_sources(app PRIVATE src/test_features.c ../ndef/src/test_validate.c)
