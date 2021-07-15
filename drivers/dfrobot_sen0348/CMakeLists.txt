cmake_minimum_required(VERSION 3.20.3)

if(CONFIG_FINGERPRINT)
    INCLUDE_DIRECTORIES(CMAKE_CURRENT_SOURCE_DIR)

    target_sources( app PRIVATE
        driver.c
    )
endif()