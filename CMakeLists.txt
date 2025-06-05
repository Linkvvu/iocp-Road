cmake_minimum_required(VERSION 3.10)
project(EchoIOCP VERSION 1.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置UTF-8编码
if(MSVC)
    add_compile_options(/utf-8)
    # add_compile_options(/W4 /WX)
    add_compile_options(/W4)
    add_definitions(-D_UNICODE -D_WINSOCK_DEPRECATED_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

# 添加源文件
set(SOURCES
    src/main.cpp
    src/IOCPServer.cpp
    src/WorkerThread.cpp
    src/Session.cpp
)

# 添加头文件
set(HEADERS
    include/IOCPServer.h
    include/IOContext.h
    include/Session.h
    include/WorkerThread.h
    include/callback.h
    include/Buffer.h
    include/log.h
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 包含头文件目录
target_include_directories(${PROJECT_NAME} PRIVATE include)

# 链接Windows Socket库
target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32) 