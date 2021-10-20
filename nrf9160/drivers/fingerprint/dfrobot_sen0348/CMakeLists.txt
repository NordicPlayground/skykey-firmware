
cmake_minimum_required(VERSION 3.16.0)

target_include_directories(app PRIVATE .)

target_sources(app PRIVATE
    driver.c
)