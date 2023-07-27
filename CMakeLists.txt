cmake_minimum_required(VERSION 3.0.0)

project(ModemChat)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# add_compile_definitions(_XOPEN_SOURCE=700)
# add_compile_definitions(_DEFAULT_SOURCE)

add_executable(chat chat.c)

add_compile_definitions(SIGTYPE=void)
add_compile_definitions(TERMIOS)
add_compile_definitions(FNDELAY=O_NDELAY)
add_compile_definitions(NO_SLEEP=)

install(TARGETS chat DESTINATION /usr/local/bin)
