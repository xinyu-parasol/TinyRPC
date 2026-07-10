# TinyRpc — 从零搭建 RPC 框架学习项目

## 什么是 RPC？

**RPC（Remote Procedure Call，远程过程调用）** 让你调用远程服务器的函数，就像调用本地函数一样。

### 没有 RPC 你要做的事

```cpp
// 你想调用远程的 Login 函数
// 没有 RPC 时，你得手动：
// 1. 把函数名 "Login" 和参数打包成字节流
// 2. 用 Socket 通过网络发出去
// 3. 对面收到后解包、调用函数、打包结果
// 4. 你收回来、解包、拿到结果
```

### 有了 RPC 之后的写法

```cpp
// 就像调用本地函数一样
LoginResponse response = stub.Login(request);
// 中间的网络传输、序列化、反序列化都由 RPC 框架做了
```

### 生活类比

> 你去餐厅点"红烧肉"：
> - **你（客户端）**：告诉服务员"一份红烧肉"
> - **服务员（Channel）**：帮你写单子（序列化）
> - **单子送到厨房（网络传输）**
> - **厨师（Provider）**：拿到单子、做菜（执行函数）
> - **做好的菜送回你面前（响应）**

---

# 项目整体架构

```
src/
├── include/
│   ├── Provider.h      # 服务端（厨房）：接收请求、调用函数、返回结果
│   ├── Channel.h        # 客户端（服务员）：发起调用、等待结果
│   ├── Controller.h     # 状态记录本：记录调用成功/失败
│   ├── Application.h    # 启动入口：初始化框架
│   ├── Config.h         # 配置文件加载
│   └── Logger.h         # 日志封装
├── Provider.cc          # ✅ 已实现（Step 3）
├── Channel.cc           # ✅ 已实现（Step 4）
├── Controller.cc        # ✅ 已实现（Step 2）
├── Application.cc       # ✅ 已实现（Step 1）
├── Config.cc            # ✅ 已实现（Step 1）
├── header.proto         # 通信协议定义
└── CMakeLists.txt
conf/
└── tinyrpc.conf         # 框架配置文件
```

### 依赖的第三方库

| 库 | 作用 |
|----|------|
| **protobuf** | 序列化/反序列化（把 C++ 对象 ↔ 二进制数据） |
| **muduo** | 高性能网络库（服务端处理 TCP 并发连接） |
| **glog** | 日志打印 |

---

# 各步骤详解

## Step 1 — Config + Application（基础设施）

### Config — 配置文件加载

把 `conf/tinyrpc.conf` 这样的文本文件读进内存：

```
rpc_server_ip=0.0.0.0
rpc_server_port=10000
```

**怎么读的：**
1. 打开文件，逐行读取
2. 去掉行首行尾空格（`Trim` 函数）
3. 跳过空行和 `#` 注释
4. 用 `=` 切分成 key 和 value
5. 存入 `unordered_map<string, string>`，用 `Load(key)` 查询

**知识点：`Trim` 为什么要去掉空格？**

配置文件可能是 `key = value`（等号两边有空格），如果不去掉，key 存成 `"rpc_server_ip "` 带了个空格，查的时候就查不到。

**知识点：`unordered_map`**

C++ 的哈希表，插入和查找平均 O(1) 复杂度。适合存 key-value 配置项。

### Application — 框架入口

**单例模式（Singleton）：** 整个程序只有一个 Application 实例。

```cpp
Application &app = Application::GetInstance();
```

**双重检查锁定：**
```cpp
if (m_instance == nullptr) {           // 第一次检查（不加锁，快）
    lock_guard<mutex> lock(m_mutex);   // 加锁
    if (m_instance == nullptr) {       // 第二次检查（防止同时创建）
        m_instance = new Application();
    }
}
```

为什么需要两次 `== nullptr`？线程 A 和 B 同时进来：
- A 进到锁里，创建了实例
- B 等锁，拿到锁后如果不检查第二次，就会再创建一个

**命令行解析（getopt）：**
```bash
./server -i conf/tinyrpc.conf
```
`-i` 后面跟配置文件路径，`getopt` 负责解析这种命令行参数格式。

### CMakeLists.txt 解析

```cmake
# 找 protobuf 库
find_package(Protobuf REQUIRED)

# 把 .proto 文件编译成 C++ 代码（生成 .pb.h 和 .pb.cc）
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# 编译成静态库
add_library(tinyrpc_core STATIC ${SRC_FILES} ${PROTO_SRCS})
```

**静态库（.a）vs 动态库（.so）：**
- `.a`：编译时直接复制进可执行文件，体积大但不需要带额外文件
- `.so`：运行时加载，体积小但部署时要带上 .so

---

## Step 2 — Controller（调用状态记录）

### Controller 的作用

每个 RPC 调用都可能出错（网络断连、服务端崩了、参数错了……）。
Controller 记录这次调用的状态：

```cpp
Controller controller;
stub.Login(&controller, &request, &response, nullptr);

if (controller.Failed()) {
    // 出错了，查原因
    cout << controller.ErrorText();
} else {
    // 正常处理 response
}
```

### 为什么返回值还不够？

| | 本地函数 | 远程调用 |
|---|---|---|
| 成功 | return 结果 | return 结果 |
| 失败 | throw 异常 或 return error_code | 网络断了？服务器挂了？超时了？这些不是业务逻辑能表达的错误 |

所以需要 Controller 这个"额外的通道"来传达传输层的错误。

### 核心接口

| 方法 | 作用 |
|------|------|
| `Reset()` | 重置状态，准备下一次调用 |
| `SetFailed(reason)` | 标记为失败，记录原因 |
| `Failed()` | 查有没有失败 |
| `ErrorText()` | 获取失败原因 |
| `StartCancel()` / `IsCanceled()` | 取消调用（本项目暂未实现） |

---

## Step 3 — Provider（RPC 服务端）

这是最核心的部分：用 muduo 启动 TCP 服务器，等待客户端发请求。

### TCP 流式协议的问题

TCP 就像一根水管，数据是一股水流进来的。
你没法直接知道"一条消息从哪里开始、到哪里结束"。

所以需要**应用层协议**来划定消息边界。

### 自定义通信协议

```
|------------ 一条完整的 RPC 请求 ------------|
[ header_size(4字节) ][ RpcHeader ] [ 请求参数(protobuf) ]
```

| 字段 | 大小 | 内容 |
|------|------|------|
| header_size | 4 字节 | 告诉接收方"RpcHeader 有多长" |
| RpcHeader | 变长（protobuf 序列化） | service_name + method_name + args_size |
| 请求参数 | 变长（protobuf 序列化） | 真正的业务参数，如 LoginRequest |

**为什么需要 header_size？**
因为 RpcHeader 是 protobuf 序列化的，长度不定。先发 4 字节告诉对方头有多长，对方才知道读多少字节来反序列化 RpcHeader。

### header.proto 定义

```protobuf
message RpcHeader {
    bytes service_name = 1;    // 服务名，如 "UserServiceRpc"
    bytes method_name = 2;     // 方法名，如 "Login"
    uint32 args_size = 3;      // 请求参数的长度
}
```

### Provider 工作流程

```
客户端连接 → OnConnection 回调（记录连接）
    ↓
数据到达 → OnMessage 回调
    ↓
┌─────────────────────────────────────────────┐
│  while (缓冲区够一条完整消息) {              │
│    ① 读 4 字节 → header_size               │
│    ② 读 RpcHeader → service_name + 方法名   │
│    ③ 读 args → 反序列化成请求对象            │
│    ④ 查 service_map → 找到注册的服务和方法   │
│    ⑤ 调用 service->CallMethod()              │
│    ⑥ 序列化响应 → conn->send() 发回客户端    │
│    ⑦ 从缓冲区移除已处理的数据                  │
│  }                                            │
└─────────────────────────────────────────────┘
```

### 关键知识点：protobuf 反射

```cpp
// 根据方法描述符，动态创建请求/响应对象
// 不需要知道具体是 LoginRequest 还是 RegisterRequest
google::protobuf::Message *request = service->GetRequestPrototype(method).New();

// 调用真正的业务函数
service->CallMethod(method, &controller, request, response, nullptr);
```

**反射**：程序在运行时"审视自己"的能力。protobuf 编译 .proto 文件时，会为每个 service 生成描述信息（方法列表、参数类型等），运行时可以通过这些信息动态调用方法。

这正是 **RPC 框架能通用的核心** —— 它不需要知道你的业务类型，全靠反射来动态创建和调用。

### muduo 核心概念

| 概念 | 类比 | 说明 |
|------|------|------|
| EventLoop | 前台总机 | 不断循环，有消息就处理，没消息就休息 |
| TcpServer | 餐厅大门 | 监听端口，接受客人连接 |
| TcpConnection | 客人 | 每个连接对应一个客户 |
| Buffer | 便签本 | 暂存收到的数据（因为数据可能还没到齐） |
| Callback | 流程表 | "有新连接时做什么"、"有新数据时做什么" |

---

## Step 4 — Channel（RPC 客户端）

### Channel 的工作

客户端的核心：发起 RPC 调用，等待结果。

### 调用流程

```
CallMethod 被调用
    ↓
① 与服务器建立 TCP 连接
    ↓
② 序列化 RpcHeader（service_name + method_name + args_size）
    ↓
③ 拼接成 [header_size(4B)][header][args] 发送
    ↓
④ 读取响应长度（4字节）
    ↓
⑤ 按长度读取响应数据
    ↓
⑥ 反序列化成 response 对象
    ↓
⑦ 关闭连接
```

### 为什么响应也要加长度前缀？

发送响应时，同样的问题是 TCP 没有消息边界。
所以服务端也先发 4 字节告诉客户端"响应有多长"，客户端再按长度读取。

```
服务端发送响应：
[ response_size(4字节) ][ 响应数据(protobuf序列化) ]
```

### 特殊标志 MSG_WAITALL

```cpp
recv(sockfd, &buf, 4, MSG_WAITALL);
```

`MSG_WAITALL` 告诉内核："我就要读 4 字节，没读够别返回"。普通 `recv` 可能只读到 1 字节就返回了（因为数据还在路上），用这个标志可以省去循环读取的麻烦。

### 和 Provider 的关系

```
                      Channel（客户端）                  Provider（服务端）
                             │                                │
         CallMethod() ───────┤                                │
              │              │                                │
              ▼              │                                │
    序列化 [header][args] ───┼──────────────────TCP─────────► │
              │              │                                │
              │              │                        OnMessage() 收到
              │              │                                │
              │              │                         反序列化 header
              │              │                                │
              │              │                         查 service_map
              │              │                                │
              │              │                         CallMethod()
              │              │                                │
              │              │                         序列化 response
              │              │                                │
              │◄─────TCP────┼────────────────── 响应数据 ────┤
              │              │                                │
       反序列化 response     │                                │
              │              │                                │
   controller->Failed()?     │                                │
              │              │                                │
    返回给调用方              │                                │
```

---

## Step 5 — 示例程序（端到端跑通）

### 架构图

```
┌──────────────────────────┐         ┌──────────────────────────┐
│  Client                  │         │  Server                  │
│  ┌───────────────────┐   │         │  ┌───────────────────┐   │
│  │ Channel.cc        │   │  TCP    │  │ Provider.cc       │   │
│  │ google::protobuf::│   │◄───────►│  │ muduo::TcpServer  │   │
│  │ RpcChannel 实现   │   │         │  │                   │   │
│  └────────┬──────────┘   │         │  └────────┬──────────┘   │
│           │              │         │           │              │
│  ┌────────▼──────────┐   │         │  ┌────────▼──────────┐   │
│  │ UserServiceRpc_   │   │         │  │ UserService       │   │
│  │ Stub (代理)       │   │         │  │ (你的业务代码)    │   │
│  └────────┬──────────┘   │         │  └───────────────────┘   │
│           │              │         │                          │
│  ┌────────▼──────────┐   │         │                          │
│  │ 你的代码:         │   │         │                          │
│  │ stub.Login()      │   │         │                          │
│  └───────────────────┘   │         │                          │
└──────────────────────────┘         └──────────────────────────┘
```

### 服务端代码（example/server/main.cc）

```cpp
class UserService : public demo::UserServiceRpc {
    void Login(..., const LoginRequest* request,
               LoginResponse* response, ...) override {
        // 你的业务逻辑
        if (name == "admin" && pwd == "123456") {
            response->set_success(true);
        }
    }
};

int main() {
    Provider provider;
    provider.NotifyService(new UserService());  // 注册服务
    provider.Run();                              // 启动服务器
}
```

### 客户端代码（example/client/main.cc）

```cpp
int main() {
    Channel channel("127.0.0.1", 10000);    // 连接到服务器
    UserServiceRpc_Stub stub(&channel);      // 创建代理

    LoginRequest request;                    // 填参数
    request.set_name("admin");
    request.set_pwd("123456");

    LoginResponse response;
    Controller controller;
    stub.Login(&controller, &request, &response, nullptr);  // 发起 RPC

    if (controller.Failed()) {
        // 处理网络层错误
    } else {
        // 处理业务结果
        printf("success=%d", response.success());
    }
}
```

### 关于 protobuf 不生成 service 类的问题

在某些 Linux 发行版（如 Fedora）上，`protoc` 编译器默认不生成 `service` 类的 C++ 代码。
表现为 `.proto` 文件中的 `service UserServiceRpc { ... }` 不会生成对应的 `UserServiceRpc` 和 `UserServiceRpc_Stub` 类。

**解决方案**：手动创建服务类（`example/user_service.h`），核心思路：

1. 继承 `google::protobuf::Service`，实现 `CallMethod`、`GetDescriptor`、`GetRequestPrototype`、`GetResponsePrototype`
2. 用 `DescriptorPool` 动态构建 `ServiceDescriptor`
3. 客户端 Stub 将调用转发到 `channel_->CallMethod()`

**这个过程帮助我们理解 protobuf 本来在背后做了什么**：.proto 文件 → protoc → C++ 类（服务和消息），现在我们把"服务"这步手动实现了。

### 运行结果

```bash
# 终端 1：启动服务端
./build/example/server/server -i conf/tinyrpc.conf

# 终端 2：运行客户端
./build/example/client/client

# 客户端输出：
Response: success=1, errcode=0, errmsg=OK

# 服务端输出：
[TinyRpc] Registered service: UserServiceRpc
[TinyRpc] RPC server starting on 0.0.0.0:10000
[UserService] Login called: name=admin, pwd=123456
```

---

## Step 6 — etcd 服务发现

### 为什么需要服务发现

之前客户端写死了服务器地址：
```cpp
Channel channel("127.0.0.1", 10000);
```

这有几个问题：
- 如果服务器换机器了，IP 变了 → 要改客户端代码
- 如果有多个服务器实例 → 无法做负载均衡
- 如果服务器挂了 → 客户端不知道

**服务发现**就是让客户端通过"服务名"找到服务器的地址，而不是硬编码 IP。

### etcd 简介

etcd 是一个分布式键值存储，用来做服务发现正合适：
- **Put(key, value, lease)**：存一个键值对，带租约（TTL）
- **Get(key)**：查一个键的值
- **Lease Grant/KeepAlive**：创建租约，定期续约

服务端注册：
```
/tinyrpc/demo.UserServiceRpc  →  "0.0.0.0:10000"  (TTL=10s, 每5秒续约)
```

客户端查询：
```
/tinyrpc/demo.UserServiceRpc  →  "0.0.0.0:10000"  → 连接过去
```

### EtcdClient 类设计

```
EtcdClient
├── RegisterService(key, value, ttl)     ← 注册服务（自动 GrantLease + Put）
├── UnregisterService(key)               ← 取消注册
├── DiscoverService(key)                 ← 发现服务
└── KeepAliveAll()                       ← 续约所有活跃租约
    ↓
底层：
├── HttpPost /v3/kv/put                  ← etcd v3 HTTP API
├── HttpPost /v3/kv/range
├── HttpPost /v3/lease/grant
└── HttpPost /v3/lease/keepalive
```

### Provider 集成

```cpp
class Provider {
    EtcdClient *m_etcd_client = nullptr;
    
    void SetEtcdClient(EtcdClient *client);  // 设置 etcd 客户端
    
    void Run() {
        // ... 启动 muduo TcpServer ...
        
        if (m_etcd_client) {
            // 注册所有服务到 etcd
            for (auto &entry : service_map) {
                m_etcd_client->RegisterService(key, addr, 10);
            }
            // 每 5 秒心跳续约
            event_loop.runEvery(5.0, &Provider::Heartbeat);
        }
        
        event_loop.loop();
    }
};
```

### Channel 集成

```cpp
class Channel {
    // 原来的直接连接方式
    Channel("127.0.0.1", 10000);
    
    // 新的服务发现方式
    Channel("demo.UserServiceRpc", etcd_client);
    
    void CallMethod(...) {
        if (m_use_discovery) {
            ResolveService();  // 从 etcd 查地址
        }
        // ... 连接并发送 RPC ...
    }
};
```

### 运行结果

```bash
# 终端 1：启动 etcd
etcd --data-dir /tmp/etcd-data

# 终端 2：启动 RPC 服务端
./build/example/server/server -i conf/tinyrpc.conf

# 终端 3：运行客户端
./build/example/client/client

# 客户端输出：
[Channel] Discovered demo.UserServiceRpc -> 0.0.0.0:10000
Response: success=1, errcode=0, errmsg=OK

# 服务端输出：
[TinyRpc] Registered service: UserServiceRpc
[TinyRpc] RPC server starting on 0.0.0.0:10000
[EtcdClient] Registered /tinyrpc/demo.UserServiceRpc -> 0.0.0.0:10000 (lease=..., TTL=10s)
[UserService] Login called: name=admin, pwd=123456
```

### 关于 etcd v3 API 的坑

etcd v3 的 HTTP 网关使用 **base64 编码**的 key 和 value，并且 lease ID 在 JSON 响应中是**字符串格式**（不是数字）：

```json
{"ID":"7587896087936508957","TTL":"10"}   ← ID 是字符串
```

所以提取时要使用：
```cpp
std::string id_str = ExtractJsonString(resp, "ID");  // 正确
// 而不是：
std::string id_str = ExtractJsonUint64(resp, "ID");  // 错误
```

---

# 文件说明

| 文件 | 作用 |
|------|------|
| `CMakeLists.txt` | 顶层构建文件，链接 muduo、protobuf、curl |
| `src/CMakeLists.txt` | 库的构建文件，编译 .cc + .proto |
| `src/header.proto` | RPC 通信协议定义 |
| `src/include/EtcdClient.h` | etcd 客户端（服务发现） |
| `src/EtcdClient.cc` | etcd v3 HTTP API 封装 |
| `conf/tinyrpc.conf` | 运行时配置 |
| `demo/user.proto` | 示例业务协议定义 |
| `example/user_service.h` | 手动实现的 protobuf 服务类 |
| `example/server/main.cc` | RPC 服务端示例（含 etcd 注册） |
| `example/client/main.cc` | RPC 客户端示例（含 etcd 发现） |
| `.gitignore` | 排除 build、IDE 等临时文件 |
