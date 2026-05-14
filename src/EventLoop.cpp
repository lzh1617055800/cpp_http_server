#include "EventLoop.h"
#include "Server.h"
#include "Connection.h" 
#include "Logger.h"
#include "ThreadPool.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <sys/epoll.h>
using namespace std;

EventLoop* EventLoop::current_loop_ = nullptr;
volatile sig_atomic_t EventLoop::stop_flag_ = 0;

EventLoop::EventLoop()
{
    epoll_fd = epoll_create(1024);
    if(epoll_fd == -1)
    {
        LOG_ERROR("epoll创建失败"+string(strerror(errno)));
        exit(EXIT_FAILURE);
    }
    events.resize(MAX_EVENTS);
    current_loop_ = this;
    LOG_INFO("epoll实例创建成功(epoll_fd=" + to_string(epoll_fd) + ")");
}

EventLoop::~EventLoop()
{
    close(epoll_fd);
    for(auto& pair:conn_map)
    {
        delete pair.second;
    }
    current_loop_ = nullptr;
}

void EventLoop::addEvent(int fd,uint32_t event, Connection* conn)
{
    struct epoll_event ep_event;
    memset(&ep_event,0,sizeof(ep_event));
    ep_event.data.fd = fd;
    ep_event.events = event | EPOLLET;//边缘触发

   if (conn_map.count(fd)) {
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ep_event);
    } else {
        conn_map[fd] = conn;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ep_event);
        LOG_INFO("fd=" + to_string(fd) + " 注册事件成功（事件类型：" + to_string(event) + ")");
    }
}

void EventLoop::removeEvent(int fd)
{
    if(conn_map.count(fd))
    {
        timer_.removeTimer(fd);
        epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,nullptr);
        delete conn_map[fd];
        conn_map.erase(fd);
        close(fd);
        LOG_INFO("fd=" + to_string(fd) + " 连接已关闭并从epoll删除");
    }
}

void EventLoop::loop()
{
    LOG_INFO("事件循环启动，等待客户端连接");
    while(!stop_flag_)
    {
        int num_events = epoll_wait(epoll_fd,events.data(),MAX_EVENTS,1000);//-1无限等待
        if(num_events == -1)
        {
            if(errno == EINTR)continue;
            LOG_ERROR("epoll等待事件失败: " + string(strerror(errno)));
            break;
        }
        auto expired_fds = timer_.tick();
        for(auto& fd:expired_fds)
        {
            LOG_INFO("fd=" + to_string(fd) + " 已超时断开连接");
            removeEvent(fd);
        }
        handleEvents(num_events);
    }
    LOG_INFO("服务器正在关闭");
}

void EventLoop::handleEvents(int num_events)
{
    for(int i = 0;i<num_events;i++)
    {
        int fd = events[i].data.fd;
        uint32_t revents = events[i].events;
        //处理新连接
        if(fd == Server::getListenfd())
        {
            handleNewConnection(fd);
        }
        //处理读事件
        else if(revents & (EPOLLIN | EPOLLPRI | EPOLLOUT))
        {
            Connection* conn = conn_map[fd];
            if(thread_pool_)
            {
                thread_pool_->submit([this,conn,revents]
                {
                    handleIOEvent(conn,revents);
                });
            }
            else{
                handleIOEvent(conn,revents);
            }

        }
        //错误
        else if(revents & EPOLLERR)
        {
            LOG_ERROR("fd=" + to_string(fd) + " 发生错误事件");
            removeEvent(fd);
        }
    }
}

void EventLoop::handleNewConnection(int listen_fd)
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    while(true)
    {
        int conn_fd = accept(listen_fd,(struct sockaddr*)&client_addr,&client_addr_len);
        if(conn_fd == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                LOG_DEBUG("所有连接已经处理完毕");
                break;
            }
            LOG_ERROR("accept连接失败: " + string(strerror(errno)));
            break;
        }
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET,&client_addr.sin_addr,client_ip,sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);
        LOG_INFO("新连接 " + string(client_ip) + ":" + to_string(client_port) + " (conn_fd=" + to_string(conn_fd) + ")");
        int flags = fcntl(conn_fd,F_GETFL,0);
        fcntl(conn_fd,F_SETFL,flags | O_NONBLOCK);
        Connection* conn = new Connection(conn_fd,this);
        addEvent(conn_fd,EPOLLIN,conn);
        timer_.addTimer(conn_fd,TIMEOUT_SEC);
    }
}

void EventLoop::handleIOEvent(Connection* conn,uint32_t revents)
{
    if(!conn)return;
    int fd = conn ->getFd();
    timer_.updateTimer(fd,TIMEOUT_SEC);
    if(revents & (EPOLLIN | EPOLLPRI))
    {
        LOG_DEBUG("fd=" + to_string(fd) + " 触发读事件");
        conn->handleRead();
    }
    else if(revents & EPOLLOUT)
    {
        LOG_DEBUG("fd=" + to_string(fd) + " 触发写事件");
        conn->handleWrite();
    }
    else if(revents &EPOLLERR)
    {
        LOG_ERROR("fd=" + to_string(fd) + " 触发错误事件");
        removeEvent(fd);
    }
}

void EventLoop::signalHandler(int signum)
{
    stop_flag_ = 1;
}

void EventLoop::setThreadPool(int num_threads)
{
    thread_pool_ = make_unique<ThreadPool>(num_threads);
}