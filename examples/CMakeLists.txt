apply_include_paths("${MEDIA_RTP_INTERNAL_INCLUDES}")
apply_include_paths("${MEDIA_RTP_EXTERNAL_INCLUDES}")

foreach(IDX 1 2 3 4 5 6 7 8)
	add_executable(example${IDX} example${IDX}.cpp)
	if (NOT MSVC OR MEDIA_RTP_COMPILE_STATIC)
		target_link_libraries(example${IDX} media_rtp-static)
	else ()
		target_link_libraries(example${IDX} media_rtp-shared)
	endif ()
endforeach(IDX)

