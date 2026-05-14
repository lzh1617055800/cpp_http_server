#ifndef CONNECTION_H
#define CONNECTION_H
#include <string>
#include "EventLoop.h"
#include "HttpRequest.h"
class EventLoop;
using namespace std;
class Connection
{
public:
    Connection(int fd, EventLoop* loop);
    ~Connection();

    void handleRead();
    void handleWrite();
    int getFd() const{return fd_;}
    string& getRecvBuf(){return recv_buf;}
    void appendSendBuf(const string& data){send_buf +=data;}
private:
    void processRequest();
    int fd_;
    EventLoop* loop;
    string recv_buf;
    string send_buf;
    bool is_closed; //标记连接是否已经关闭
    HttpRequest req;
};
#endif