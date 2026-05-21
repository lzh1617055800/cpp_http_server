# cpp_http_server

一个基于 `C++17 + epoll + Reactor + 线程池` 实现的轻量级 HTTP 服务器项目。

## 项目特点

- 支持 `GET` 静态文件访问
- 支持 `POST` 请求回显
- 基于 `epoll` 的事件驱动模型
- 使用线程池处理 I/O 事件
- 使用定时器清理超时连接
- 提供简单日志模块，便于调试

## 技术栈

- `C++17`
- `Linux / Unix`
- `epoll`
- `Socket`
- `Reactor`
- `线程池`
- `CMake`

## 目录结构

```text
cpp_http_server/
├── CMakeLists.txt
├── include/          # 头文件
├── src/              # 源码
├── wwwroot/          # 静态资源目录
└── README.md
```

## 功能说明

### 1. 静态资源服务

当浏览器访问 `GET /` 时，服务器默认返回 `wwwroot/index.html`。  
访问其他路径时，会尝试读取 `wwwroot` 下对应文件并返回。

### 2. POST 请求处理

当前版本对 `POST` 请求采用回显方式处理，会把请求体封装到 HTML 中返回，方便调试和验证请求内容。

### 3. 长连接与超时处理

- 采用非阻塞 socket
- 使用 `epoll` 监听读写事件
- 使用定时器清理长时间无活动的连接

## 构建方式

> 该项目面向 Linux 环境开发和运行。

```bash
mkdir -p build
cd build
cmake ..
make
```

构建后可执行文件位于：

```text
build/http_server
```

## 运行方式

在项目根目录下运行：

```bash
./build/http_server
```

默认监听：

```text
0.0.0.0:8080
```

然后用浏览器访问：

```text
http://127.0.0.1:8080/
```

## 示例请求

### GET

```bash
curl http://127.0.0.1:8080/
```

### POST

```bash
curl -X POST http://127.0.0.1:8080/ -d "hello=world"
```

## 实现思路

这个项目的核心流程大致是：

1. `Server` 创建监听 socket 并初始化 `EventLoop`
2. `EventLoop` 通过 `epoll` 监听新连接和 I/O 事件
3. `Connection` 负责读取请求、解析 HTTP、生成响应
4. `ThreadPool` 负责分发部分 I/O 处理任务
5. `Timer` 定期清理超时连接

## 当前已知特点

- 这是一个学习型项目，偏重于理解 HTTP、Reactor 和 Linux 网络编程
- 静态资源目录为 `wwwroot`
- 服务器默认端口为 `8080`

## 后续可以继续优化的方向

- 支持更多 HTTP 方法和状态码
- 增加更完整的 HTTP 解析
- 改进静态资源路径安全性
- 增加压力测试和单元测试
- 支持更完善的 keep-alive 逻辑

## 作者

`lzh1617055800`
