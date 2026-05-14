#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H
#include "Timer.h"
#include <vector>
#include <unordered_map>
#include <sys/epoll.h>
#include <csignal>
#include <memory>
using namespace std;

class Connection;
class ThreadPool;
class EventLoop
{
public:
    EventLoop();
    ~EventLoop();
    void loop();
    void addEvent(int fd,uint32_t event,Connection* conn );
    void removeEvent(int fd);
    static EventLoop* getCurrentLoop(){return current_loop_;}
    static void signalHandler(int signum);
    void setThreadPool(int num_threads);
    
private:
    void handleEvents(int num_events);
    void handleNewConnection(int lishen_fd);
    void handleIOEvent(Connection* conn,uint32_t revents);
    int epoll_fd;
    vector<struct epoll_event> events;
    unordered_map<int,Connection*> conn_map;
    static const int MAX_EVENTS = 1024;
    static EventLoop* current_loop_;
    Timer timer_;
    static const int TIMEOUT_SEC = 15;
    static volatile sig_atomic_t stop_flag_;
    unique_ptr<ThreadPool> thread_pool_;
};
#endif