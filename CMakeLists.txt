cmake_minimum_required(VERSION 3.17)
project(TestServer C)

set(CMAKE_C_STANDARD 99)

add_executable(TestServer main.c warp.h warp.c epoll_reactor.c)