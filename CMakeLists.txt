cmake_minimum_required(VERSION 3.0.0)

project(ModemChat)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(chat chat.c)
