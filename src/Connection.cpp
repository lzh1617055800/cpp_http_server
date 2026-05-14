#include "Connection.h"
#include "EventLoop.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Logger.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <fstream>

using namespace std;
Connection::Connection(int fd, EventLoop* loop)
    :fd_(fd),loop(loop){is_closed =false;}

Connection::~Connection()
{
    is_closed = true;
    LOG_INFO("Connection对象销毁(fd=" + to_string(fd_) + ")");
}

void Connection::handleRead()
{
    char buf[1024] = {0};
    while(1)
    {
        ssize_t n = read(fd_ ,buf,sizeof(buf)-1);
        if(n>0)
        {
            recv_buf.append(buf,n);
            memset(buf,0,sizeof(buf));
            LOG_DEBUG("fd=" + to_string(fd_) + " 读取数据" + recv_buf);
        }
        else if(n == 0)//对端关闭了连接
        {
            LOG_INFO("fd=" + to_string(fd_) + " 客户端关闭连接");
            loop->removeEvent(fd_);
            return;
        }
        else
        {
            if(errno == EAGAIN || errno ==EWOULDBLOCK)
            {
                LOG_DEBUG("fd=" + to_string(fd_) + " 读取数据完毕(ET模式)");
                // 循环处理缓冲区中可能存在的多个请求（Keep-Alive）
                while(!recv_buf.empty())
                {
                    if(req.parse(recv_buf))
                    {
                        processRequest();
                        // 计算已处理的请求字节数
                        size_t header_end = recv_buf.find("\r\n\r\n");
                        if(header_end != string::npos)
                        {
                            // 请求体长度 = Content-Length 头的值（如果没有就是 0）
                            string content_len_str = req.getHeader("Content-Length");
                            size_t body_len = 0;
                            if(!content_len_str.empty())
                            {
                                body_len = stoi(content_len_str);
                            }
                            size_t consumed = header_end + 4 + body_len;
                            if(consumed > recv_buf.size())
                            {
                                consumed = recv_buf.size();
                            }
                            recv_buf.erase(0, consumed);
                        }
                        else
                        {
                            recv_buf.clear();
                        }
                        // 重置 HttpRequest，准备解析下一个请求
                        req = HttpRequest();
                    }
                    else
                    {
                        // 解析失败，可能是数据不完整或格式错误
                        string response = "HTTP/1.1 400 Bad Request\r\n\r\nInvalid Request";
                        appendSendBuf(response);
                        loop->addEvent(fd_, EPOLLOUT, this);
                        recv_buf.clear();
                        break;
                    }
                }
                return;
            }
            LOG_ERROR("fd=" + to_string(fd_) + " 读数据失败" + string(strerror(errno)));
            loop->removeEvent(fd_);
            return;
        }
    }
}

void Connection::processRequest() {
    string method_str;
    if(req.getMethod() == Method::GET)
    {
        method_str = "GET";
    }
    else if(req.getMethod() == Method::POST)
    {
        method_str = "POST";
    }
    else{
        method_str = "UNKNOWN";
    }
    LOG_INFO("方法 " + method_str + " 路径 " + req.getPath());
    if(req.getMethod() == Method::GET)
    {
        string urlpath = req.getPath();
        if(urlpath == "/")
        {
            urlpath = "/index.html";
        }
        string wwwroot =  "/home/lzh/Desktop/cpp_http_server/wwwroot";
        string filepath = wwwroot + urlpath;

        HttpResponse resp;
        ifstream file(filepath,ios::binary | ios::ate);
        if(!file.is_open())
        {
            //404
            resp.setStatusCode(404);
            resp.setHeader("Content-Type", "text/html; charset=utf-8");
            string errorBody = "<h1>404 Not Found</h1><p>The requested resource was not found.</p>";
            resp.setBody(errorBody);
        }
        else{
            resp.setStatusCode(200);
            resp.setHeader("Content-Type", HttpResponse::getMimeType(filepath)); 
            streamsize size = file.tellg();
            file.seekg(0,ios::beg);
            string content(size,'\0');
            file.read(&content[0],size);
            resp.setBody(content);
        }
        
        appendSendBuf(resp.serialize());
        loop->addEvent(fd_, EPOLLOUT | EPOLLIN, this);
    }
    else if(req.getMethod() == Method::POST)
    {
        HttpResponse resp;
        resp.setStatusCode(200);
        resp.setHeader("Content-Type", "text/html; charset=utf-8");
        
        // 把客户端发来的请求体包装成 HTML 返回
        string html = "<h1>POST 请求已收到</h1>";
        html += "<p>你提交的数据：</p>";
        html += "<pre>" + req.getBody() + "</pre>";
        resp.setBody(html);
        
        appendSendBuf(resp.serialize());
        loop->addEvent(fd_, EPOLLOUT | EPOLLIN, this);
    }
    else{
        HttpResponse resp;
        resp.setStatusCode(405);
        resp.setHeader("Content-Type", "text/html; charset=utf-8");
        resp.setBody("<h1>405 Method Not Allowed</h1>");
        appendSendBuf(resp.serialize());
        loop->addEvent(fd_, EPOLLOUT | EPOLLIN, this);
    }
}

    
void Connection::handleWrite()
{
    if(send_buf.empty())
    {

        return;
    }
    while(!send_buf.empty())
    {
        ssize_t n = write(fd_,send_buf.data(),send_buf.size());
        if(n>0)     
        {
            send_buf.erase(0,n);
            LOG_DEBUG("fd=" + to_string(fd_) + " 发送数据" + to_string(n) + "字节，剩余" + to_string(send_buf.size()) + "字节");
        }
        else
        {
            if(errno == EAGAIN ||errno == EWOULDBLOCK)
            {
                LOG_DEBUG("fd=" + to_string(fd_) + " 暂时无法发送，等待下次写事件");
                return;
            }
            else
            {
                LOG_ERROR("fd=" + to_string(fd_) + " 发送数据失败: " + string(strerror(errno)));
                loop->removeEvent(fd_);
                return;
            }

        }
    }
}