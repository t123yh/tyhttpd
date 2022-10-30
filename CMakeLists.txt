cmake_minimum_required(VERSION 3.23)
project(tyhttpd C)

set(CMAKE_C_STANDARD 99)

add_executable(tyhttpd main.c stream.c http.c http.h utils.c utils.h)
