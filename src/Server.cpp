#include "Server.h"
#include "EventLoop.h"
#include "Logger.h"
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <cstring>

using namespace std;

int Server::listen_fd= -1;
Server::Server(int port, const string& ip)
        :port_(port),ip_(ip),event_loop(nullptr)
{
    memset(&server_addr_,0,sizeof(server_addr_ ));
}
Server::~Server()
{
    if(listen_fd !=-1)
    {
        close(listen_fd);
        LOG_INFO("监听Socket已经关闭(fd=" + to_string(listen_fd) + ")");
    }
    delete event_loop;
}

int Server::createListenfd()
{
    int fd =socket(AF_INET,SOCK_STREAM,0);
    if(fd == -1)
    {
        LOG_ERROR("创建socket失败: " + string(strerror(errno)));
        return -1;
    }
    int opt = 1;
    //SO_REUSEADDR 复用本地地址和端口 
    //SO_REUSERPORT 允许多个进程和线程绑定到同一个端口
    //opt= 1表示启用上面两个选项
    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT,&opt,sizeof(opt));
    return fd;
}   

void Server::setNonBlocking(int fd)
{
    //获取当前fd的文件状态标志 
    //file control  F_GETFL 获取当前文件状态 get file flags
    int flags = fcntl(fd,F_GETFL,0);
    if(flags == -1)
    {
        LOG_ERROR("设置非阻塞失败: " + string(strerror(errno)));
        return;
    }
    fcntl(fd,F_SETFL,flags | O_NONBLOCK);
    LOG_INFO("socket(fd=" + to_string(fd) + ") 已设置为非阻塞模式");
}

bool Server::init()
{
    listen_fd = createListenfd();
    if(listen_fd == -1)
    {
        return false;
    }

    setNonBlocking(listen_fd);

    server_addr_.sin_family=AF_INET;
    server_addr_.sin_port = htons(port_);
    server_addr_.sin_addr.s_addr = inet_addr(ip_.c_str());

    if(bind(listen_fd, (struct sockaddr*)&server_addr_,sizeof(server_addr_)) == -1)
    {
       LOG_ERROR("绑定端口" + to_string(port_) + "失败: " + string(strerror(errno)));
        close(listen_fd);
        listen_fd = -1;
        return false;
    }

    if(listen(listen_fd,128) == -1)
    {
        LOG_ERROR("监听端口" + to_string(port_) + "失败: " + string(strerror(errno)));
        close(listen_fd);
        listen_fd = -1;
        return false;
    }

    event_loop = new EventLoop();
    //EPOLLIN表示关注的类型,即表示有新的客户端连接请求到来
    event_loop ->addEvent(listen_fd,EPOLLIN,nullptr);
    LOG_INFO("服务器初始化成功，监听 " + ip_ + ":" + to_string(port_));
    return true;
}

void Server::start()
{
    if(event_loop)
    {
        event_loop->setThreadPool(4);
        event_loop->loop();
    }
}
