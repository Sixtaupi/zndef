# Copyright (c) 2026 Romain Pelletant
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)
project(zndef VERSION 0.1.0 LANGUAGES C)

if(CONFIG_ZNDEF)
  set(ZNDEF_DIR ${CMAKE_CURRENT_LIST_DIR})

  zephyr_library_named(zndef)
  # The library is plain ISO C99, without extensions.
  zephyr_library_compile_options(-std=c99 -Wpedantic)
  zephyr_include_directories(${ZNDEF_DIR}/include)
  zephyr_library_sources(
    ${ZNDEF_DIR}/src/ndef.c
    ${ZNDEF_DIR}/src/ndef_type.c
    ${ZNDEF_DIR}/src/ndef_writer.c)
  zephyr_library_sources_ifdef(CONFIG_ZNDEF_VALIDATE_RTD ${ZNDEF_DIR}/src/ndef_validate.c)
  zephyr_library_sources_ifdef(CONFIG_ZNDEF_RTD_TEXT ${ZNDEF_DIR}/src/rtd_text.c)
  zephyr_library_sources_ifdef(CONFIG_ZNDEF_RTD_URI ${ZNDEF_DIR}/src/rtd_uri.c)
  zephyr_library_sources_ifdef(CONFIG_ZNDEF_RTD_SP ${ZNDEF_DIR}/src/rtd_sp.c)
  zephyr_library_sources_ifdef(CONFIG_ZNDEF_RTD_SIG ${ZNDEF_DIR}/src/rtd_sig.c)
  zephyr_library_sources_ifdef(CONFIG_ZNDEF_RTD_HANDOVER ${ZNDEF_DIR}/src/rtd_handover.c)
  zephyr_library_sources_ifdef(CONFIG_ZNDEF_RTD_DI ${ZNDEF_DIR}/src/rtd_di.c)
  zephyr_library_sources_ifdef(CONFIG_ZNDEF_ITEMS ${ZNDEF_DIR}/src/ndef_items.c)
  zephyr_library_sources_ifdef(CONFIG_ZNDEF_TLV ${ZNDEF_DIR}/src/tlv.c)
endif()
