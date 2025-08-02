# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 工作流程

1. First think through the problem, read the codebase for relevant files, and write a plan to tasks/todo.md.
2. The plan should have a list of todo items that you can check off as you complete them
3. Before you begin working, check in with me and I will verify the plan.
4. Then, begin working on the todo items, marking them as complete as you go.
5. Please every step of the way just give me a high level explanation of what changes you made
6. Make every task and code change you do as simple as possible. We want to avoid making any massive or complex changes. Every change should impact as little code as possible. Everything is about simplicity.
7. Finally, add a review section to the [todo.md](http://todo.md/) file with a summary of the changes you made and any other relevant information.
8. Respond in Chinese.

## Project Overview

JRTPLIB is a C++ object-oriented library implementing the Real-time Transport Protocol (RTP) as described in RFC 3550. This is a legacy library that is **no longer actively maintained** as announced by the original author. The library provides high-level RTP session management, RTCP handling, and multiple transmission protocols.

## Build System

The project uses CMake as its build system. Basic build commands:

```bash
# Create build directory
mkdir build && cd build

# Configure project
cmake ..

# Build library and examples  
make

# Build with specific options
cmake -DJRTPLIB_COMPILE_EXAMPLES=ON -DJRTPLIB_COMPILE_TESTS=ON ..
```

### Important CMake Options

- `JRTPLIB_COMPILE_EXAMPLES=ON/OFF` - Build example programs (default: ON)
- `JRTPLIB_COMPILE_TESTS=ON/OFF` - Build test programs (default: OFF)
- `JTHREAD_ENABLED=ON/OFF` - Enable thread support via JThread
- `SRTP_ENABLED=ON/OFF` - Enable SRTP support via libsrtp
- `SRTP2_ENABLED=ON/OFF` - Enable SRTP2 support via libsrtp2

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

- **Namespace**: All classes are in `jrtplib` namespace
- **Memory Management**: Optional custom memory managers via `RTPMemoryManager`
- **Error Handling**: Integer return codes (negative = error), use `RTPGetErrorString()`
- **Thread Safety**: Library is NOT thread-safe by design - use external locking

## Development Workflow

### Building Examples
Examples are in `examples/` directory and demonstrate library usage:
- `example1.cpp` - Basic IPv4 RTP sending
- `example2.cpp` - Complete send/receive application  
- `example7.cpp` - SRTP usage example

### Platform-Specific Notes

- **Windows**: Winsock2 support, define `WIN32`
- **Android**: Cross-compilation toolchain setup in README2.md
- **IPv6**: Conditional compilation based on system support

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

- **Optional**: JThread library for background processing
- **Optional**: libsrtp/libsrtp2 for SRTP support  
- **Required**: Standard C++ library, platform sockets API

## Security Notes

This is a library for implementing RTP protocol. When working with network protocols:
- Validate all input packet data
- Be aware of potential buffer overflows in packet parsing
- SRTP functionality requires proper key management
- Library does not handle authentication/authorization

## Common Gotchas

- Library is not thread-safe - must use external synchronization
- Call `BeginDataAccess()`/`EndDataAccess()` around data operations
- SRTP and SRTP2 cannot be enabled simultaneously  
- Default endianness detection may need override for cross-compilation
- Memory management system requires consistent use of library's allocation/deallocation
