cmake_minimum_required(VERSION 3.21)
project(tyhttpd C)

set(CMAKE_C_STANDARD 99)

find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
  include_directories(${OPENSSL_INCLUDE_DIRS})
  message(STATUS "OpenSSL Found!")
endif()
set(OPENSSL_USE_STATIC_LIBS TRUE)

add_executable(tyhttpd main.c stream.c tlstream.c http.c http.h utils.c utils.h)
target_link_libraries(tyhttpd OpenSSL::Crypto OpenSSL::SSL)
