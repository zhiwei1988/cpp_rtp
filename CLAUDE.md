# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 工作流程

1. 首先分析问题，阅读相关文件，制定计划并使用TodoWrite工具跟踪任务
2. 计划应包含可检查完成状态的待办事项清单
3. 开始工作前，与用户确认计划
4. 执行待办事项，逐步标记完成状态
5. 每个步骤都提供高层次的变更说明
6. 保持每个任务和代码变更尽可能简单，避免大规模复杂变更，最小化代码影响范围
7. 所有变更完成后验证构建和测试是否通过
8. 使用中文回复

## Project Overview

This is a modified version of JRTPLIB - a C++ object-oriented library implementing the Real-time Transport Protocol (RTP) as described in RFC 3550. The original library has been transformed into a custom implementation with copyright notices removed. The library provides high-level RTP session management, RTCP handling, and multiple transmission protocols including UDP IPv4/IPv6 and TCP.

**Project Status**: This is now a custom RTP implementation (media_rtp) derived from the original JRTPLIB codebase, with all original copyright and license information removed as documented in tasks/todo.md.

## Build System

The project uses CMake as its build system. Basic build commands:

```bash
# Create build directory
mkdir build && cd build

# Configure project (Linux only - no Windows/winsock support)
cmake ..

# Build library and examples  
make

# Build with specific options
cmake -DMEDIA_RTP_COMPILE_TESTS=ON ..
```

### Current Configuration
- **Platform**: Linux only (Windows/winsock support disabled)
- **Library Name**: media_rtp (changed from jrtplib)
- **Version**: 3.11.2

### Important CMake Options

- `MEDIA_RTP_COMPILE_TESTS=ON/OFF` - Build test programs in tests/ directory (default: OFF)
- `JTHREAD_ENABLED=ON/OFF` - Enable thread support via JThread library (default: based on availability)
- `MEDIA_RTP_USE_BIGENDIAN=ON/OFF` - Set target platform endianness for cross-compilation
- `MEDIA_RTP_SUPPORT_*` - Various feature toggles (SDESPRIV, PROBATION, SENDAPP, etc.)

## Core Architecture

### Primary Components

1. **RTPSession** (`src/rtpsession.h/.cpp`) - Main high-level class for RTP applications
   - Handles RTCP internally
   - Manages packet sending/receiving
   - For SRTP, use RTPSecureSession instead

2. **RTPTransmitter** (`src/rtptransmitter.h`) - Abstract base for transmission protocols
   - `RTPUDPv4Transmitter` - UDP over IPv4 (most common)
   - `RTPUDPv6Transmitter` - UDP over IPv6  
   - `RTPTCPTransmitter` - TCP transmission
   - `RTPExternalTransmitter` - Custom external mechanisms

3. **RTPPacket/RTPRawPacket** - Packet handling and parsing
4. **RTCP Classes** (`rtcp*.h/.cpp`) - RTCP packet types and scheduling
5. **RTPSources** - Source/participant management

### Key Design Patterns

- **Memory Management**: Optional custom memory managers via `RTPMemoryManager`
- **Error Handling**: Integer return codes (negative = error), use `RTPGetErrorString()`  
- **Thread Safety**: Library is NOT thread-safe by design - use external locking
- **Session Management**: Use `BeginDataAccess()`/`EndDataAccess()` around data operations

## Development Workflow

### Running Tests
Tests are located in `tests/` directory:
```bash
# Build with tests enabled
cmake -DMEDIA_RTP_COMPILE_TESTS=ON ..
make

# Available test programs:
./tests/comprehensive_udp_test    # Main UDP functionality test
./tests/tcptest                   # TCP transmission test
./tests/testautoportbase         # Auto port selection test
./tests/testrawpacket            # Raw packet handling test
./tests/rtcpdump                 # RTCP packet analysis
./tests/abortdesctest            # Abort descriptor test
./tests/abortdescipv6            # IPv6 abort descriptor test
```

### Platform-Specific Notes

- **Linux Only**: This version supports Linux only (Windows/winsock disabled)
- **IPv6**: Conditional compilation based on system support
- **Cross-compilation**: Use `MEDIA_RTP_USE_BIGENDIAN` for endianness control

### Configuration Headers

Generated headers in build directory:
- `rtpconfig.h` - Platform and feature detection
- `rtptypes.h` - Type definitions (uint32_t, etc.)
- `rtplibraryversioninternal.h` - Version information

## Code Conventions

- Header guards: `#ifndef FILENAME_H`
- Include order: config headers first, then system, then library headers
- Error codes: Use `RTPGetErrorString()` for user-friendly messages
- Memory: Use `session.DeletePacket()` for RTPPacket cleanup

## Dependencies

- **Optional**: JThread library for background processing (thread support)
- **Required**: Standard C++ library, Linux sockets API
- **Build**: CMake 2.8 or later

## Security Notes

This is a library for implementing RTP protocol. When working with network protocols:
- Validate all input packet data
- Be aware of potential buffer overflows in packet parsing  
- Library does not handle authentication/authorization
- External locking required for thread safety

## Common Gotchas

- Library is NOT thread-safe - must use external synchronization
- Call `BeginDataAccess()`/`EndDataAccess()` around data operations
- Default endianness detection may need override for cross-compilation  
- Memory management system requires consistent use of library's allocation/deallocation
- Use `session.DeletePacket()` to properly deallocate RTPPacket instances

## File Structure & Navigation

### Core Implementation
- **`src/rtpsession.h/.cpp`** - Main RTPSession class (primary API entry point)
- **`src/rtp*transmitter.h/.cpp`** - Transmission layer implementations (UDP IPv4/IPv6, TCP)
- **`src/rtppacket*.h/.cpp`** - RTP packet handling and building
- **`src/rtcp*.h/.cpp`** - RTCP protocol implementation and packet types
- **`src/rtpsources.h/.cpp`** - RTP source/participant management

### Build & Configuration
- **`CMakeLists.txt`** - Main build configuration
- **`src/rtpconfig.h.in`** - Platform feature detection template
- **`cmake/`** - Build system modules and platform detection

### Testing & Validation
- **`tests/`** - Comprehensive test suite (enable with `MEDIA_RTP_COMPILE_TESTS=ON`)
- **`tools/`** - Platform detection utilities for build configuration

### Documentation & Tracking
- **`tasks/todo.md`** - Project modification tracking and development plans
- **`CLAUDE.md`** - This guidance file for Claude Code instances
