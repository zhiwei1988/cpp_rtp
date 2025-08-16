add_definitions(-DMEDIA_RTP_COMPILING)

set (HEADERS
	media_rtcp_packet_factory.h
	media_rtcp_scheduler.h
	media_rtcp_packet_factory.h
	media_rtp_collisionlist.h
	${PROJECT_BINARY_DIR}/src/rtpconfig.h
	media_rtp_defines.h
	media_rtp_errors.h
	media_rtp_packet.h
	media_rtp_packet_builder.h
	media_rtp_pollthread.h
	media_rtp_utils.h
	media_rtp_raw_packet.h
	media_rtp_session.h
	media_rtp_session_params.h
	media_rtp_source_data.h
	media_rtp_sources.h
	media_rtp_structs.h
	media_rtp_transmitter.h
	media_rtp_udpv4_transmitter.h
	media_rtp_udpv6_transmitter.h  
	media_rtp_endpoint.h
	media_rtp_abort_descriptors.h
	media_rtp_tcp_transmitter.h	
)

set(SOURCES
	media_rtcp_packet_factory.cpp
	media_rtcp_scheduler.cpp
	media_rtp_collisionlist.cpp
	media_rtp_packet.cpp
	media_rtp_packet_builder.cpp
	media_rtp_pollthread.cpp
	media_rtp_utils.cpp
	media_rtp_session.cpp
	media_rtp_session_params.cpp
	media_rtp_source_data.cpp
	media_rtp_sources.cpp
	media_rtp_udpv4_transmitter.cpp
	media_rtp_udpv6_transmitter.cpp 
	media_rtp_endpoint.cpp
	media_rtp_abort_descriptors.cpp
	media_rtp_tcp_transmitter.cpp
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

