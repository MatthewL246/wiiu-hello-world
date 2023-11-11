cmake_minimum_required(VERSION 3.2)
set(CMAKE_TOOLCHAIN_FILE $ENV{DEVKITPRO}/cmake/WiiU.cmake)

project(helloworld C)

add_executable(helloworld main.c)

target_compile_options(helloworld PRIVATE -Wall -Wextra -pedantic)

wut_create_rpx(helloworld)
