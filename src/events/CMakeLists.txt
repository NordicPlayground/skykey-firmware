#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_sources(app PRIVATE
	       ${CMAKE_CURRENT_SOURCE_DIR}/display_module_event.c
		   ${CMAKE_CURRENT_SOURCE_DIR}/cloud_module_event.c
		   ${CMAKE_CURRENT_SOURCE_DIR}/download_module_event.c
		   ${CMAKE_CURRENT_SOURCE_DIR}/password_module_event.c
		   ${CMAKE_CURRENT_SOURCE_DIR}/modem_module_event.c
		   ${CMAKE_CURRENT_SOURCE_DIR}/lock_module_event.c
		   )