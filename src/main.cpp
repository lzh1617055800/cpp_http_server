#include "Server.h"
#include "EventLoop.h"
#include "Logger.h"
#include <iostream>
#include <csignal>
using namespace std;
int main()
{
    signal(SIGINT,EventLoop::signalHandler);
    signal(SIGTERM,EventLoop::signalHandler);
    Server server(8080);
    if(!server.init())
    {
        LOG_ERROR("服务器初始化失败");
        return -1;
    }
    server.start();
    return 0;
}
