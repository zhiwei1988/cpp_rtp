apply_include_paths("${MEDIA_RTP_INTERNAL_INCLUDES}")
apply_include_paths("${MEDIA_RTP_EXTERNAL_INCLUDES}")

foreach(T testmultiplex testexistingsockets testautoportbase rtcpdump readlogfile
	  timetest timeinittest abortdesctest abortdescipv6 tcptest sigintrtest
	  testexttrans testrawpacket)
	add_executable(${T} ${T}.cpp)
	if (NOT MSVC OR MEDIA_RTP_COMPILE_STATIC)
		target_link_libraries(${T} media_rtp-static)
	else ()
		target_link_libraries(${T} media_rtp-shared)
	endif ()
endforeach(T)

