add_definitions(-DMEDIA_RTP_COMPILING)

set (HEADERS
    packets/media_rtcp_packet_factory.h
    core/media_rtcp_scheduler.h
    core/media_rtp_collisionlist.h
    ${PROJECT_BINARY_DIR}/src/rtpconfig.h
    utils/media_rtp_defines.h
    utils/media_rtp_errors.h
    packets/media_rtp_packet.h
    packets/media_rtp_packet_factory.h
    utils/media_rtp_pollthread.h
    utils/media_rtp_utils.h
    core/media_rtp_session.h
    core/media_rtp_session_params.h
    core/media_rtp_source_data.h
    core/media_rtp_sources.h
    utils/media_rtp_structs.h
    transmitters/media_rtp_transmitter.h
    transmitters/media_rtp_udpv4_transmitter.h
    transmitters/media_rtp_udpv6_transmitter.h  
    utils/media_rtp_endpoint.h
    core/media_rtp_abort_descriptors.h
    transmitters/media_rtp_tcp_transmitter.h 
)

set(SOURCES
    packets/media_rtcp_packet_factory.cpp
    core/media_rtcp_scheduler.cpp
    core/media_rtp_collisionlist.cpp
    packets/media_rtp_packet_factory.cpp
    utils/media_rtp_pollthread.cpp
    utils/media_rtp_utils.cpp
    core/media_rtp_session.cpp
    core/media_rtp_session_params.cpp
    core/media_rtp_source_data.cpp
    core/media_rtp_sources.cpp
    transmitters/media_rtp_udpv4_transmitter.cpp
    transmitters/media_rtp_udpv6_transmitter.cpp 
    utils/media_rtp_endpoint.cpp
    core/media_rtp_abort_descriptors.cpp
    transmitters/media_rtp_tcp_transmitter.cpp
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

apply_include_paths("${MEDIA_RTP_INTERNAL_INCLUDES}")
apply_include_paths("${MEDIA_RTP_EXTERNAL_INCLUDES}")

install(FILES ${HEADERS} DESTINATION include/media_rtp)
install(TARGETS ${MEDIA_RTP_INSTALLTARGETS} DESTINATION ${LIBRARY_INSTALL_DIR})

