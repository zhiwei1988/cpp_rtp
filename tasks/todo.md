# 删除版权声明和许可证信息 - 任务清单

## 项目概述
将jrtplib转换为自研代码，需要删除所有版权声明、许可证信息和法律文本，只保留技术实现相关的注释。

## 分析结果
经过搜索，发现189个文件包含版权相关内容。主要包括：
1. 源码文件(.h/.cpp) - 在文件开头有完整的MIT许可证声明
2. 示例文件(examples/) - 部分有简短的说明注释  
3. 测试文件(tests/) - 包含版权声明
4. 配置文件 - 部分包含版权信息
5. LICENSE.MIT文件 - 独立的许可证文件
6. build/目录下的生成文件

## 版权声明模式
典型的版权声明块包括：
- "This file is a part of JRTPLIB"
- "Copyright (c) 1999-2017 Jori Liesenborgs"  
- 联系信息和机构信息
- MIT许可证完整文本(Permission is hereby granted...)

## 执行计划

### 阶段1: 删除独立许可证文件
- [ ] 删除 LICENSE.MIT 文件

### 阶段2: 处理源码文件 (src/ 目录)
- [ ] 删除所有.h文件开头的版权声明块(约40个文件)
- [ ] 删除所有.cpp文件开头的版权声明块(约30个文件)  
- [ ] 删除extratransmitters/目录下的版权声明

### 阶段3: 处理示例文件 (examples/ 目录)
- [ ] 检查并清理所有example*.cpp文件的版权信息

### 阶段4: 处理测试文件 (tests/ 目录)  
- [ ] 删除所有测试文件中的版权声明

### 阶段5: 处理配置和文档文件
- [ ] 清理CMakeLists.txt和配置文件中的版权信息
- [ ] 清理doc/目录下的文档文件
- [ ] 检查并清理其他配置文件

### 阶段6: 清理build目录生成文件
- [ ] 清理build/目录下包含版权信息的生成文件(如果需要)

### 阶段7: 最终验证
- [ ] 再次搜索确认所有版权相关内容已清除
- [ ] 确保技术注释和功能说明被保留
- [ ] 验证代码编译无问题

## 处理原则
1. **彻底删除**: 删除所有版权声明、许可证文本、作者信息
2. **保留技术内容**: 保留文件描述、技术注释、功能说明
3. **保持结构**: 保持文件的基本结构和功能完整性
4. **分批处理**: 按文件类型分批处理，便于跟踪进度

## 注意事项
- 删除版权块后需保留文件的技术描述（如 \file 标记）
- 确保删除后代码结构和功能不受影响
- 重点关注文件开头的完整MIT许可证文本块
- 保留必要的技术说明和API文档注释

---

# RTPMemoryObject 自定义内存管理重构计划

## 问题分析
RTPMemoryObject 类存在过度设计问题：
- 15个核心类继承该基类，增加复杂度
- 25个文件使用 RTPNew/RTPDelete 宏
- 实际项目中没有自定义内存管理器实现
- 运行时开销：每个对象存储额外的内存管理器指针

## 重构目标
简化内存管理，移除自定义内存管理机制，使用标准 C++ new/delete

## 重构计划

### 阶段1: 禁用自定义内存管理 (低风险验证)
- [ ] 修改 CMakeLists.txt，将 MEDIA_RTP_SUPPORT_MEMORYMGMT 默认改为 OFF
- [ ] 重新构建项目验证编译通过
- [ ] 运行基本测试确保功能正常

### 阶段2: 移除继承关系和宏 (主要重构) - ✅ 已完成
- [x] 移除15个类对 RTPMemoryObject 的继承关系：
  - RTPSession, RTPPacket, RTPTransmitter, RTPSources 等
- [x] 删除构造函数中的 RTPMemoryManager *mgr 参数
- [x] 替换所有 RTPNew/RTPDelete 宏为标准 new/delete
- [x] 移除 GetMemoryManager() 和 SetMemoryManager() 方法

## 实际执行记录

### 已完成的修改
1. **核心类继承关系移除**:
   - rtpsourcedata.h/.cpp - 移除 RTPMemoryObject 继承
   - rtpsources.h/.cpp - 移除 RTPMemoryObject 继承  
   - rtptransmitter.h - 移除 RTPMemoryObject 继承
   - rtppacket.h/.cpp - 移除 RTPMemoryObject 继承
   - rtppacketbuilder.h/.cpp - 移除 RTPMemoryObject 继承
   - rtpsession.h/.cpp - 移除 RTPMemoryObject 继承
   - rtcpsdesinfo.h - 移除 RTPMemoryObject 继承
   - rtcpcompoundpacket.h/.cpp - 移除 RTPMemoryObject 继承
   - rtcpcompoundpacketbuilder.h - 移除 RTPMemoryObject 继承
   - rtcppacketbuilder.h/.cpp - 移除 RTPMemoryObject 继承

2. **地址类修复**:
   - 修复所有 RTPAddress 子类的 CreateCopy 方法，移除 mgr 参数
   - 更新 rtpbyteaddress, rtpipv4address, rtpipv6address, rtptcpaddress

3. **传输器类修复**:
   - 修复所有 transmitter 类的构造函数，移除 mgr 参数
   - RTPUDPv4Transmitter, RTPUDPv6Transmitter, RTPTCPTransmitter

4. **调用代码更新**:
   - 更新所有调用这些类的代码，移除 mgr 参数
   - 修复编译错误和警告

### 验证结果
- ✅ 编译完全成功：项目和所有测试都能正常编译
- ✅ 功能测试通过：comprehensive_udp_test 所有测试通过 (7/7, 100%成功率)
- ✅ 无内存管理器依赖：代码现在使用标准 C++ new/delete

### 阶段3: 清理代码 (彻底清除) - 待完成
- [ ] 删除 src/rtpmemoryobject.h 文件
- [ ] 删除 src/rtpmemorymanager.h 文件  
- [ ] 清理所有 #ifdef RTP_SUPPORT_MEMORYMANAGEMENT 条件编译代码
- [ ] 移除 CMakeLists.txt 中的 MEDIA_RTP_SUPPORT_MEMORYMGMT 选项
- [ ] 更新相关头文件的 #include 语句

### 阶段4: 测试和验证 - ✅ 已完成
- [x] 运行完整测试套件确保无回归问题
- [x] 验证所有示例程序正常编译和运行
- [x] 检查内存泄漏和性能影响

## 影响文件列表
### 核心继承类文件
- rtpsession.h/.cpp
- rtppacket.h/.cpp  
- rtptransmitter.h
- rtpsources.h/.cpp
- rtppacketbuilder.h/.cpp
- rtcppacketbuilder.h/.cpp
- rtcpcompoundpacketbuilder.h
- rtprawpacket.h
- rtpsourcedata.h/.cpp
- rtcpcompoundpacket.h/.cpp
- rtcpsdesinfo.h
- rtpcollisionlist.h/.cpp

### 使用 RTPNew/RTPDelete 的文件 (25个)
需要逐一替换为标准 new/delete

## 风险评估
- **低风险**: 代码已支持禁用自定义内存管理
- **需要全面测试**: 确保无回归问题
- **建议**: 保留git分支作为备份

## 预期收益
- 简化代码结构，提高可维护性
- 减少运行时内存开销
- 移除复杂的条件编译逻辑
- 更符合现代C++最佳实践

---

# RTP地址系统重构计划

## 问题分析
当前地址系统存在严重冗余：
- 13个文件实现相似功能：Address类 + Destination类重复实现
- 概念混淆：Address(抽象地址) vs Destination(具体socket地址)
- 重复代码：IPv4/IPv6地址信息在两套类中都有存储
- 接口不一致：构造函数参数、命名规范不统一
- 过度设计：复杂继承体系，实际只需简单的端点概念

## 重构目标
设计单一、简洁的RTPEndpoint类，消除继承，统一地址表示

## 重构方案

### 新设计：单一RTPEndpoint类
```cpp
class RTPEndpoint {
public:
    enum Type { IPv4, IPv6, TCP };
    
    // IPv4构造
    RTPEndpoint(uint32_t ip, uint16_t rtpPort, uint16_t rtcpPort = 0);
    RTPEndpoint(const uint8_t ip[4], uint16_t rtpPort, uint16_t rtcpPort = 0);
    
    // IPv6构造  
    RTPEndpoint(const in6_addr& ip, uint16_t rtpPort, uint16_t rtcpPort = 0);
    RTPEndpoint(const uint8_t ip[16], uint16_t rtpPort, uint16_t rtcpPort = 0);
    
    // TCP构造
    RTPEndpoint(SocketType socket);
    
    // 统一接口(小驼峰命名)
    Type getType() const;
    bool isSameEndpoint(const RTPEndpoint& other) const;
    bool isSameHost(const RTPEndpoint& other) const;
    
    // IPv4/IPv6专用
    uint32_t getIPv4() const;           // IPv4专用
    const in6_addr& getIPv6() const;    // IPv6专用
    uint16_t getRtpPort() const;
    uint16_t getRtcpPort() const;
    
    // TCP专用
    SocketType getSocket() const;       // TCP专用
    
    // Socket地址获取
    const sockaddr* getRtpSockAddr() const;
    const sockaddr* getRtcpSockAddr() const;
    socklen_t getSockAddrLen() const;
};
```

### 文件结构简化
```
删除文件(13个):
- rtpaddress.h/.cpp
- rtpbyteaddress.h/.cpp  
- rtpipv4address.h/.cpp
- rtpipv6address.h/.cpp
- rtptcpaddress.h/.cpp
- rtpipv4destination.h/.cpp
- rtpipv6destination.h/.cpp

新增文件(1个):
- rtpendpoint.h/.cpp
```

## 重构计划

### 阶段1: 基础清理
- [ ] 删除RTPByteAddress相关文件(rtpbyteaddress.h/.cpp)
- [ ] 设计新的RTPEndpoint类接口(采用小驼峰命名)

### 阶段2: 实现新类
- [ ] 实现新的RTPEndpoint类
- [ ] 支持IPv4/IPv6/TCP三种端点类型
- [ ] 内部存储优化，避免重复数据

### 阶段3: 替换老接口
- [ ] 搜索所有使用RTPIPv4Address的代码并替换
- [ ] 搜索所有使用RTPIPv6Address的代码并替换  
- [ ] 搜索所有使用RTPTCPAddress的代码并替换
- [ ] 搜索所有使用RTPIPv4Destination的代码并替换
- [ ] 搜索所有使用RTPIPv6Destination的代码并替换

### 阶段4: 清理工作
- [ ] 删除所有旧的地址类文件
- [ ] 更新CMakeLists.txt构建配置
- [ ] 清理相关的#include语句

### 阶段5: 验证测试
- [ ] 验证编译通过
- [ ] 运行comprehensive_udp_test验证IPv4功能
- [ ] 运行IPv6相关测试验证IPv6功能  
- [ ] 运行TCP相关测试验证TCP功能
- [ ] 检查无内存泄漏

## 风险控制
- 不考虑向后兼容，直接替换所有老接口
- 分阶段进行，每阶段都要验证编译通过
- 保持git分支备份，出问题可以回滚

## 预期收益
- 文件数量从13个减少到1个(92%减少)
- 消除重复代码和概念混淆
- 统一接口风格(小驼峰命名)
- 简化维护工作