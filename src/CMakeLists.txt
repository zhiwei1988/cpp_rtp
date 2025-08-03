add_definitions(-DMEDIA_RTP_COMPILING)

set (HEADERS
	rtcpapppacket.h
	rtcpbyepacket.h
	rtcpcompoundpacket.h
	rtcpcompoundpacketbuilder.h
	rtcppacket.h
	rtcppacketbuilder.h
	rtcprrpacket.h
	rtcpscheduler.h
	rtcpsdesinfo.h
	rtcpsdespacket.h
	rtcpsrpacket.h
	rtcpunknownpacket.h
	rtpaddress.h
	rtpcollisionlist.h
	${PROJECT_BINARY_DIR}/src/rtpconfig.h
	rtpdefines.h
	rtperrors.h
	rtpinternalsourcedata.h
	rtpipv4address.h
	rtpipv4destination.h
	rtpipv6address.h
	rtpipv6destination.h
	rtppacket.h
	rtppacketbuilder.h
	rtppollthread.h
	rtp_protocol_utils.h
	rtprawpacket.h
	rtpsession.h
	rtpsessionparams.h
	rtpsessionsources.h
	rtpsourcedata.h
	rtpsources.h
	rtpstructs.h
	rtptimeutilities.h
	rtptransmitter.h
	${PROJECT_BINARY_DIR}/src/rtptypes.h
	rtpudpv4transmitter.h
	rtpudpv6transmitter.h  
	rtpbyteaddress.h
	rtpsocketutil.h
	rtpabortdescriptors.h
	rtpselect.h
	rtptcpaddress.h
	rtptcptransmitter.h
	)

set(SOURCES
	rtcpapppacket.cpp
	rtcpbyepacket.cpp
	rtcpcompoundpacket.cpp
	rtcpcompoundpacketbuilder.cpp
	rtcppacket.cpp
	rtcppacketbuilder.cpp
	rtcprrpacket.cpp
	rtcpscheduler.cpp
	rtcpsdesinfo.cpp
	rtcpsdespacket.cpp
	rtcpsrpacket.cpp
	rtpcollisionlist.cpp
	rtpinternalsourcedata.cpp
	rtpipv4address.cpp
	rtpipv6address.cpp
	rtpipv4destination.cpp
	rtpipv6destination.cpp
	rtppacket.cpp
	rtppacketbuilder.cpp
	rtppollthread.cpp
	rtp_protocol_utils.cpp
	rtpsession.cpp
	rtpsessionparams.cpp
	rtpsessionsources.cpp
	rtpsourcedata.cpp
	rtpsources.cpp
	rtptimeutilities.cpp
	rtpudpv4transmitter.cpp
	rtpudpv6transmitter.cpp 
	rtpbyteaddress.cpp
	rtpabortdescriptors.cpp
	rtptcpaddress.cpp
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

