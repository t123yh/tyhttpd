cmake_minimum_required(VERSION 3.23)
project(myhttpd C)

set(CMAKE_C_STANDARD 99)

add_executable(myhttpd main.c stream.c http.c http.h utils.c utils.h)
