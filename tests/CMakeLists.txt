cmake_minimum_required(VERSION 3.10)

# 包含 CMake 的 FetchContent 模块
# FetchContent 是 CMake 3.11 引入的一个模块，用于在配置时下载外部依赖
include(FetchContent)

# 添加Google Test
# 声明要下载的 Google Test 库
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git # 指定了 Google Test 的 Git 仓库地址
    GIT_TAG release-1.12.1 # 指定了要下载的版本标签
)

# 下载 Google Test 源代码，配置 Google Test 项目，并将其添加到当前项目中
FetchContent_MakeAvailable(googletest)

set(PACKETS_TEST_SOURCES
  test_rtp_raw_packet.cpp
  test_rtp_packet_builder.cpp
  test_rtp_packet.cpp
  test_rtcp_packets.cpp
)

add_executable(packets_tests ${PACKETS_TEST_SOURCES})

target_link_libraries(packets_tests
  PRIVATE
    GTest::gtest
    GTest::gtest_main
    media_rtp-static
    pthread
)

target_include_directories(packets_tests
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/utils
    ${PROJECT_SOURCE_DIR}/src/packets
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_BINARY_DIR}/src
)

# Test executable can be run directly: ./packets_tests
