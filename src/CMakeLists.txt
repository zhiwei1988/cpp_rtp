add_definitions(-DMEDIA_RTP_COMPILING)

# 核心会话管理头文件
set(CORE_HEADERS
	core/media_rtcp_scheduler.h
	core/media_rtp_abort_descriptors.h
	core/media_rtp_collisionlist.h
	core/media_rtp_session.h
	core/media_rtp_session_params.h
	core/media_rtp_source_data.h
	core/media_rtp_sources.h
)

# 数据包处理头文件
set(PACKETS_HEADERS
	packets/media_rtcp_packet_factory.h
	packets/media_rtp_packet_factory.h
)

# 传输器头文件
set(TRANSMITTERS_HEADERS
	transmitters/media_rtp_transmitter.h
	transmitters/media_rtp_udpv4_transmitter.h
	transmitters/media_rtp_udpv6_transmitter.h
	transmitters/media_rtp_tcp_transmitter.h
)

# 工具类头文件
set(UTILS_HEADERS
	utils/media_rtp_utils.h
	utils/media_rtp_defines.h
	utils/media_rtp_errors.h
	utils/media_rtp_structs.h
	utils/media_rtp_endpoint.h
	utils/media_rtp_pollthread.h
	${PROJECT_BINARY_DIR}/src/rtpconfig.h
)

# 合并所有头文件
set(HEADERS
	${CORE_HEADERS}
	${PACKETS_HEADERS}
	${TRANSMITTERS_HEADERS}
	${UTILS_HEADERS}
)

# 核心会话管理源文件
set(CORE_SOURCES
	core/media_rtp_session.cpp
	core/media_rtcp_scheduler.cpp
	core/media_rtp_abort_descriptors.cpp
	core/media_rtp_collisionlist.cpp
	core/media_rtp_session_params.cpp
	core/media_rtp_source_data.cpp
	core/media_rtp_sources.cpp
)

# 数据包处理源文件
set(PACKETS_SOURCES
	packets/media_rtcp_packet_factory.cpp
	packets/media_rtp_packet_factory.cpp
)

# 传输器源文件
set(TRANSMITTERS_SOURCES
	transmitters/media_rtp_udpv4_transmitter.cpp
	transmitters/media_rtp_udpv6_transmitter.cpp
	transmitters/media_rtp_tcp_transmitter.cpp
)

# 工具类源文件
set(UTILS_SOURCES
	utils/media_rtp_utils.cpp
	utils/media_rtp_endpoint.cpp
	utils/media_rtp_pollthread.cpp
)

# 合并所有源文件
set(SOURCES
	${CORE_SOURCES}
	${PACKETS_SOURCES}
	${TRANSMITTERS_SOURCES}
	${UTILS_SOURCES}
)


#Temporarily disable warnings to test build
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_GNUCC)
	option(MEDIA_RTP_WARNINGSASERRORS "Enable -Wall -Wextra -Werror" OFF)
	if (MEDIA_RTP_WARNINGSASERRORS)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
	else ()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
	endif ()
endif(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_GNUCC)

# Linux static library
add_library(media_rtp-static STATIC ${SOURCES} ${HEADERS})
set_target_properties(media_rtp-static PROPERTIES OUTPUT_NAME media_rtp)
set_target_properties(media_rtp-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set(MEDIA_RTP_INSTALLTARGETS media_rtp-static)
target_link_libraries(media_rtp-static ${MEDIA_RTP_LINK_LIBS})

# Linux shared library
add_library(media_rtp-shared SHARED ${SOURCES} ${HEADERS})
set_target_properties(media_rtp-shared PROPERTIES OUTPUT_NAME media_rtp)
set_target_properties(media_rtp-shared PROPERTIES VERSION ${VERSION})
set_target_properties(media_rtp-shared PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set(MEDIA_RTP_INSTALLTARGETS ${MEDIA_RTP_INSTALLTARGETS} media_rtp-shared)
target_link_libraries(media_rtp-shared ${MEDIA_RTP_LINK_LIBS})

# 添加各个子目录到include路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/packets)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/transmitters)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/utils)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

apply_include_paths("${MEDIA_RTP_INTERNAL_INCLUDES}")
apply_include_paths("${MEDIA_RTP_EXTERNAL_INCLUDES}")

install(FILES ${HEADERS} DESTINATION include/media_rtp)
install(TARGETS ${MEDIA_RTP_INSTALLTARGETS} DESTINATION ${LIBRARY_INSTALL_DIR})
