#include "Connection.h"
#include "EventLoop.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Logger.h"
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>

using namespace std;

namespace
{
string trim(const string& value)
{
    const string whitespace = " \t\r\n";
    size_t begin = value.find_first_not_of(whitespace);
    if(begin == string::npos)
    {
        return "";
    }
    size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

size_t extractContentLength(const string& request)
{
    size_t header_end = request.find("\r\n\r\n");
    if(header_end == string::npos)
    {
        return 0;
    }

    istringstream iss(request.substr(0, header_end));
    string line;

    if(!getline(iss, line))
    {
        return 0;
    }

    while(getline(iss, line))
    {
        if(!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if(line.empty())
        {
            break;
        }

        size_t colon = line.find(':');
        if(colon == string::npos)
        {
            continue;
        }

        string key = trim(line.substr(0, colon));
        string value = trim(line.substr(colon + 1));
        if(key == "Content-Length")
        {
            try
            {
                return static_cast<size_t>(stoul(value));
            }
            catch(...)
            {
                return 0;
            }
        }
    }

    return 0;
}
}

Connection::Connection(int fd, EventLoop* loop)
    : fd_(fd), loop(loop), is_closed(false)
{
}

Connection::~Connection()
{
    is_closed = true;
    LOG_INFO("connection destroyed, fd=" + to_string(fd_));
}

void Connection::handleRead()
{
    char buf[4096];

    while(true)
    {
        ssize_t n = read(fd_, buf, sizeof(buf));
        if(n > 0)
        {
            recv_buf.append(buf, static_cast<size_t>(n));
        }
        else if(n == 0)
        {
            LOG_INFO("fd=" + to_string(fd_) + " client closed connection");
            loop->removeEvent(fd_);
            return;
        }
        else
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            LOG_ERROR("fd=" + to_string(fd_) + " read failed: " + string(strerror(errno)));
            loop->removeEvent(fd_);
            return;
        }
    }

    while(true)
    {
        size_t header_end = recv_buf.find("\r\n\r\n");
        if(header_end == string::npos)
        {
            break;
        }

        size_t content_length = extractContentLength(recv_buf);
        size_t request_len = header_end + 4 + content_length;
        if(recv_buf.size() < request_len)
        {
            break;
        }

        string request_text = recv_buf.substr(0, request_len);
        if(!req.parse(request_text))
        {
            string response =
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                "Content-Length: 11\r\n"
                "\r\n"
                "Bad Request";
            appendSendBuf(response);
            loop->addEvent(fd_, EPOLLOUT, this);
            recv_buf.clear();
            return;
        }

        processRequest();
        recv_buf.erase(0, request_len);
        req = HttpRequest();
    }
}

void Connection::processRequest()
{
    string method_str;
    if(req.getMethod() == Method::GET)
    {
        method_str = "GET";
    }
    else if(req.getMethod() == Method::POST)
    {
        method_str = "POST";
    }
    else
    {
        method_str = "UNKNOWN";
    }

    LOG_INFO("method=" + method_str + " path=" + req.getPath());

    if(req.getMethod() == Method::GET)
    {
        string urlpath = req.getPath();
        if(urlpath == "/")
        {
            urlpath = "/index.html";
        }

        const string wwwroot = "wwwroot";
        string filepath = wwwroot + urlpath;

        HttpResponse resp;
        ifstream file(filepath, ios::binary | ios::ate);
        if(!file.is_open())
        {
            resp.setStatusCode(404);
            resp.setHeader("Content-Type", "text/html; charset=utf-8");
            resp.setBody("<h1>404 Not Found</h1><p>The requested resource was not found.</p>");
        }
        else
        {
            resp.setStatusCode(200);
            resp.setHeader("Content-Type", HttpResponse::getMimeType(filepath));
            streamsize size = file.tellg();
            file.seekg(0, ios::beg);
            string content(static_cast<size_t>(size), '\0');
            file.read(&content[0], size);
            resp.setBody(content);
        }

        appendSendBuf(resp.serialize());
        loop->addEvent(fd_, EPOLLIN | EPOLLOUT, this);
    }
    else if(req.getMethod() == Method::POST)
    {
        HttpResponse resp;
        resp.setStatusCode(200);
        resp.setHeader("Content-Type", "text/html; charset=utf-8");

        string html = "<h1>POST request received</h1>";
        html += "<p>Request body:</p>";
        html += "<pre>" + req.getBody() + "</pre>";
        resp.setBody(html);

        appendSendBuf(resp.serialize());
        loop->addEvent(fd_, EPOLLIN | EPOLLOUT, this);
    }
    else
    {
        HttpResponse resp;
        resp.setStatusCode(405);
        resp.setHeader("Content-Type", "text/html; charset=utf-8");
        resp.setBody("<h1>405 Method Not Allowed</h1>");
        appendSendBuf(resp.serialize());
        loop->addEvent(fd_, EPOLLIN | EPOLLOUT, this);
    }
}

void Connection::handleWrite()
{
    if(send_buf.empty())
    {
        loop->addEvent(fd_, EPOLLIN, this);
        return;
    }

    while(!send_buf.empty())
    {
        ssize_t n = write(fd_, send_buf.data(), send_buf.size());
        if(n > 0)
        {
            send_buf.erase(0, static_cast<size_t>(n));
        }
        else
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }

            LOG_ERROR("fd=" + to_string(fd_) + " send failed: " + string(strerror(errno)));
            loop->removeEvent(fd_);
            return;
        }
    }

    loop->addEvent(fd_, EPOLLIN, this);
}
