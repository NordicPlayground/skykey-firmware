#
# Copyright (c) 2019-2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_include_directories(app PRIVATE .)
target_sources_ifdef(CONFIG_DISPLAY_MODULE app PRIVATE 
${CMAKE_CURRENT_SOURCE_DIR}/lvgl_interface.c
${CMAKE_CURRENT_SOURCE_DIR}/assets/nordic_semi_text_w150px.c
${CMAKE_CURRENT_SOURCE_DIR}/assets/nordic_semi_w150px.c
${CMAKE_CURRENT_SOURCE_DIR}/assets/fingerprint_w100px.c
)