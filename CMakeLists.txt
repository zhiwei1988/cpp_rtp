cmake_minimum_required(VERSION 2.8)
if (POLICY CMP0042)
	cmake_policy(SET CMP0042 NEW)
endif()

project(media_rtp)
set(VERSION_MAJOR 3)
set(VERSION_MINOR 11)
set(VERSION_DEBUG 2)
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_DEBUG}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")
if (CMAKE_INSTALL_PREFIX AND NOT CMAKE_PREFIX_PATH)
    #message("Setting CMAKE_PREFIX_PATH to ${CMAKE_INSTALL_PREFIX}")
    file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}" CMAKE_PREFIX_PATH)
endif()

set (_DEFAULT_LIBRARY_INSTALL_DIR lib)
if (EXISTS "${CMAKE_INSTALL_PREFIX}/lib32/" AND CMAKE_SIZEOF_VOID_P EQUAL 4)
	set (_DEFAULT_LIBRARY_INSTALL_DIR lib32)
elseif (EXISTS "${CMAKE_INSTALL_PREFIX}/lib64/" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
	set (_DEFAULT_LIBRARY_INSTALL_DIR lib64)
endif ()

set(LIBRARY_INSTALL_DIR "${_DEFAULT_LIBRARY_INSTALL_DIR}" CACHE PATH "Library installation directory")
if(NOT IS_ABSOLUTE "${LIBRARY_INSTALL_DIR}")
	set(LIBRARY_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${LIBRARY_INSTALL_DIR}")
endif()

include(CheckCXXCompilerFlag)
include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckCXXSourceCompiles)
include(TestBigEndian)
include(${PROJECT_SOURCE_DIR}/cmake/Macros.cmake)


set(MEDIA_RTP_LINK_LIBS "")
set(MEDIA_RTP_INTERNAL_INCLUDES "")
set(MEDIA_RTP_EXTERNAL_INCLUDES "")

add_additional_stuff(MEDIA_RTP_EXTERNAL_INCLUDES MEDIA_RTP_LINK_LIBS)

media_rtp_support_option("Support the probation mechanism for a new source" MEDIA_RTP_SUPPORT_PROBATION RTP_SUPPORT_PROBATION ON "// Do not wait for a number of consecutive packets to validate source")
media_rtp_support_option("Support sending RTCP APP packets" MEDIA_RTP_SUPPORT_SENDAPP RTP_SUPPORT_SENDAPP ON "// No direct support for sending RTCP APP packets")
media_rtp_support_option("Support sending unknown RTCP packets" MEDIA_RTP_SUPPORT_RTCPUNKNOWN RTP_SUPPORT_RTCPUNKNOWN OFF "// No support for sending unknown RTCP packets")

media_rtp_include_test(sys/filio.h RTP_HAVE_SYS_FILIO "// Don't have <sys/filio.h>")
media_rtp_include_test(sys/sockio.h RTP_HAVE_SYS_SOCKIO "// Don't have <sys/sockio.h>")
media_rtp_include_test(netinet/in.h RTP_SUPPORT_NETINET_IN "// Don't have <netinet/in.h>")

# Always enable thread support using C++ standard library
set (RTP_SUPPORT_THREAD "#define RTP_SUPPORT_THREAD")
# Add pthread library for thread support
save_paths(MEDIA_RTP_LINK_LIBS "pthread")

if (CMAKE_CROSSCOMPILING)
	option (MEDIA_RTP_USE_BIGENDIAN "Target platform is big endian" ON)
	if (MEDIA_RTP_USE_BIGENDIAN)
		set(RTP_ENDIAN "#define RTP_BIG_ENDIAN")
	else (MEDIA_RTP_USE_BIGENDIAN)
		set(RTP_ENDIAN "// Little endian system")
	endif (MEDIA_RTP_USE_BIGENDIAN)
else (CMAKE_CROSSCOMPILING)
	test_big_endian(MEDIA_RTP_BIGENDIAN)
	if (MEDIA_RTP_BIGENDIAN)
		set(RTP_ENDIAN "#define RTP_BIG_ENDIAN")
	else (MEDIA_RTP_BIGENDIAN)
		set(RTP_ENDIAN "// Little endian system")
	endif (MEDIA_RTP_BIGENDIAN)
endif (CMAKE_CROSSCOMPILING)

option(MEDIA_RTP_COMPILE_TESTS "Compile various tests in the 'tests' subdirectory" YES)

# Linux only - no winsock support
set(TESTDEFS "")
set(MEDIA_RTP_WINSOCK FALSE)
set(RTP_SOCKETTYPE_WINSOCK "// Not using winsock sockets")

media_rtp_test_feature(socklentest RTP_SOCKLENTYPE_UINT TRUE "// socklen_t is 'int'" "${TESTDEFS}")
media_rtp_test_feature(ipv4mcasttest RTP_SUPPORT_IPV4MULTICAST FALSE "// No IPv4 multicasting support" "${TESTDEFS}")
media_rtp_test_feature(salentest RTP_HAVE_SOCKADDR_LEN FALSE "// No sa_len member in struct sockaddr" "${TESTDEFS}")
media_rtp_test_feature(getloginrtest RTP_SUPPORT_GETLOGINR FALSE "// Not using getlogin_r" "${TESTDEFS}")
media_rtp_test_feature(ipv6test RTP_SUPPORT_IPV6 FALSE "// No IPv6 support" "${TESTDEFS}")
media_rtp_test_feature(ipv6mcasttest RTP_SUPPORT_IPV6MULTICAST FALSE "// No IPv6 multicasting support" "${TESTDEFS}")
# Linux - no Windows specific features
set(RTP_HAVE_QUERYPERFORMANCECOUNTER "// No QueryPerformanceCounter support")
set(RTP_HAVE_VSUINT64SUFFIX "// No ui64 suffix")
media_rtp_test_feature(arrayalloc RTP_HAVE_ARRAYALLOC FALSE "// No custom new[]" "${TESTDEFS}")
set(RTP_HAVE_RAND_S "// No rand_s support")
set(RTP_HAVE_STRNCPY_S "// No strncpy_s support")
media_rtp_test_feature(clockgettimetest RTP_HAVE_CLOCK_GETTIME FALSE "// No clock_gettime support" "${TESTDEFS}")
media_rtp_test_feature(polltest RTP_HAVE_POLL FALSE "// No 'poll' support" "${TESTDEFS}")
set(RTP_HAVE_WSAPOLL "// No 'WSAPoll' support")
media_rtp_test_feature(msgnosignaltest RTP_HAVE_MSG_NOSIGNAL FALSE "// No MSG_NOSIGNAL option" "${TESTDEFS}")
media_rtp_test_feature(ifaddrstest RTP_SUPPORT_IFADDRS FALSE "// No ifaddrs support" "${TESTDEFS}")

# Linux uses standard snprintf
set(RTP_SNPRINTF_VERSION "// Stdio snprintf version")

check_cxx_source_compiles("#include <stdint.h>\n#include <sys/types.h>\nint main(void) { size_t a = 0 ; size_t b = a; uint32_t x = 0; uint32_t y = x; return 0; }" MEDIA_RTP_STDINT)
if (MEDIA_RTP_STDINT)
	set(RTP_INTTYPE_HEADERS "#include <stdint.h>\n#include <sys/types.h>")
else (MEDIA_RTP_STDINT)
	check_cxx_source_compiles("#include <inttypes.h>\n#include <sys/types.h>\nint main(void) { uint32_t x = 0; uint32_t y = x; return 0; }" MEDIA_RTP_INTTYPES)
	if (MEDIA_RTP_INTTYPES)
		set(RTP_INTTYPE_HEADERS "#include <inttypes.h>\n#include <sys/types.h>\n")
	else (MEDIA_RTP_INTTYPES)
			set(RTP_INTTYPE_HEADERS "#error Could not find header files that define types like 'uint32_t'. Please edit the file ${PROJECT_BINARY_DIR}/src/rtptypes_unix.h and make sure that the following types are defined: int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t")
			message("\n\nError: Could not find header files that define types like 'uint32_t'.\nPlease edit the file ${PROJECT_BINARY_DIR}/src/rtptypes_unix.h\nand make sure that the following types are defined: \nint8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t\n\n")
	endif (MEDIA_RTP_INTTYPES)
endif (MEDIA_RTP_STDINT)

# Linux - no winsock headers needed
set(RTP_WINSOCK_HEADERS "")


# Linux - no import/export decoration needed (removed unused macros)

configure_file("${PROJECT_SOURCE_DIR}/src/utils/rtpconfig.h.in" "${PROJECT_BINARY_DIR}/src/rtpconfig.h")

save_paths(MEDIA_RTP_INTERNAL_INCLUDES
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/src"
    "${PROJECT_SOURCE_DIR}/src/utils"
    "${PROJECT_SOURCE_DIR}/src/packets"
    "${PROJECT_SOURCE_DIR}/src/core"
    "${PROJECT_SOURCE_DIR}/src/transmitters"
)

add_subdirectory(src)

if (MEDIA_RTP_COMPILE_TESTS)
	add_subdirectory(tests)
endif()

# Linux library configuration
set(MEDIA_RTP_LIBS "-L${LIBRARY_INSTALL_DIR}" "-lmedia_rtp")

set(MEDIA_RTP_INCDIRS ${MEDIA_RTP_EXTERNAL_INCLUDES} ${CMAKE_INSTALL_PREFIX}/include)
set(MEDIA_RTP_LIBS ${MEDIA_RTP_LIBS} ${MEDIA_RTP_LINK_LIBS})
remove_empty(MEDIA_RTP_INCDIRS)
list(REMOVE_DUPLICATES MEDIA_RTP_INCDIRS)
remove_empty(MEDIA_RTP_LIBS)

foreach(ARG ${MEDIA_RTP_LIBS})
	set(MEDIA_RTP_LIBS_CMAKECONFIG "${MEDIA_RTP_LIBS_CMAKECONFIG} \"${ARG}\"")
endforeach()
foreach(ARG ${MEDIA_RTP_INCDIRS})
	set(MEDIA_RTP_INCDIRS_CMAKECONFIG "${MEDIA_RTP_INCDIRS_CMAKECONFIG} \"${ARG}\"")
endforeach()

configure_file("${PROJECT_SOURCE_DIR}/cmake/MediaRTPConfig.cmake.in" "${PROJECT_BINARY_DIR}/cmake/MediaRTPConfig.cmake")
install(FILES "${PROJECT_BINARY_DIR}/cmake/MediaRTPConfig.cmake" DESTINATION ${LIBRARY_INSTALL_DIR}/cmake/MediaRTP)

# Linux pkgconfig support
foreach(ARG ${MEDIA_RTP_LIBS})
	set(MEDIA_RTP_LIBS_PKGCONFIG "${MEDIA_RTP_LIBS_PKGCONFIG} ${ARG}")
endforeach()
foreach(ARG ${MEDIA_RTP_INCDIRS})
	set(MEDIA_RTP_INCDIRS_PKGCONFIG "${MEDIA_RTP_INCDIRS_PKGCONFIG} -I${ARG}")
endforeach()


