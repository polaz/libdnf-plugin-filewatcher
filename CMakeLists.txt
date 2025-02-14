cmake_minimum_required(VERSION 3.12)
project(filewatcher_plugin)

set(CMAKE_CXX_STANDARD 20)  # Используем C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(/usr/include/dnf5 /usr/include/libdnf5)

add_library(filewatcher MODULE filewatcher.cpp)
# Указываем только "dnf5", так как библиотека имеет имя файла libdnf5.so (линковка производится как -ldnf5)
target_link_libraries(filewatcher PRIVATE dnf5)

