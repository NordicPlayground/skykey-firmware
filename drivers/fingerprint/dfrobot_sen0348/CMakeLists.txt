
cmake_minimum_required(VERSION 3.20.3)

target_include_directories(app PRIVATE .)

target_sources(app PRIVATE
    driver.c
)
message(STATUS "SEN0348 driver.c added\n")