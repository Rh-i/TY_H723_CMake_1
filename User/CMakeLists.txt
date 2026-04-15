set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

enable_language(C CXX ASM)

add_library(User_lib OBJECT)

target_include_directories(User_lib PUBLIC
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/Bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/Device
    ${CMAKE_CURRENT_SOURCE_DIR}/Module
    ${CMAKE_CURRENT_SOURCE_DIR}/Middleware/Algorithm
    ${CMAKE_CURRENT_SOURCE_DIR}/Middleware/Service
    ${CMAKE_CURRENT_SOURCE_DIR}/App
)
target_link_libraries(User_lib PRIVATE stm32cubemx)

# 包含所有的用户.c .cpp
file(GLOB_RECURSE USER_SOURCE
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS  # 关键：启用依赖检查

    ../User/*.c
    ../User/*.cpp
)
target_sources(User_lib PRIVATE ${USER_SOURCE})


