cmake_minimum_required(VERSION 3.16.0)

if(CONFIG_FINGERPRINT)
    target_include_directories(app PRIVATE .)

    if (CONFIG_SEN0348)
        add_subdirectory(dfrobot_sen0348)
    endif()

endif()