add_definitions(-DMEDIA_RTP_COMPILING)

set (HEADERS
	media_rtcp_packet_factory.h
	media_rtcp_scheduler.h
	media_rtcp_packet_factory.h
	rtpcollisionlist.h
	${PROJECT_BINARY_DIR}/src/rtpconfig.h
	rtpdefines.h
	rtperrors.h
	rtppacket.h
	rtppacketbuilder.h
	rtppollthread.h
	media_rtp_utils.h
	rtprawpacket.h
	rtpsession.h
	rtpsessionparams.h
	rtpsourcedata.h
	rtpsources.h
	rtpstructs.h
	rtptransmitter.h
	rtpudpv4transmitter.h
	rtpudpv6transmitter.h  
	rtpendpoint.h
	rtpabortdescriptors.h
	rtptcptransmitter.h
	)

set(SOURCES
	media_rtcp_packet_factory.cpp
	media_rtcp_scheduler.cpp
	rtpcollisionlist.cpp
	rtppacket.cpp
	rtppacketbuilder.cpp
	rtppollthread.cpp
	media_rtp_utils.cpp
	rtpsession.cpp
	rtpsessionparams.cpp
	rtpsourcedata.cpp
	rtpsources.cpp
	rtpudpv4transmitter.cpp
	rtpudpv6transmitter.cpp 
	rtpendpoint.cpp
	rtpabortdescriptors.cpp
	rtptcptransmitter.cpp
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

