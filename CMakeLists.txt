cmake_minimum_required(VERSION 3.19)
project(codec-string C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_FLAGS "-g")
#set(CMAKE_EXECUTABLE_SUFFIX ".html") # 编译生成.html

add_executable(codec-string main.c)
#set_target_properties(codec-string PROPERTIES LINK_FLAGS "-s EXPORTED_RUNTIME_METHODS=['ccall']")
