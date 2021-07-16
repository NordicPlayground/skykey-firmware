cmake_minimum_required(VERSION 3.20.3)

if(CONFIG_FINGERPRINT)
    target_include_directories(app PRIVATE .)

    if (CONFIG_SEN0348)
        message(STATUS "Adding sen0348 directory\n")
        add_subdirectory(dfrobot_sen0348)
    endif()

endif()