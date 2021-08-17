#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_include_directories(app PRIVATE .)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/modules_common.c)
target_sources_ifdef(CONFIG_DISPLAY_MODULE app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/display_module.c)
target_sources_ifdef(CONFIG_CLOUD_MODULE app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cloud_module.c)
target_sources_ifdef(CONFIG_DOWNLOAD_MODULE app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/download_module.c)
target_sources_ifdef(CONFIG_PASSWORD_MODULE app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/password_module.c)
target_sources_ifdef(CONFIG_FINGERPRINT_MODULE app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fingerprint_module.c)
target_sources_ifdef(CONFIG_MODEM_MODULE app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/modem_module.c)