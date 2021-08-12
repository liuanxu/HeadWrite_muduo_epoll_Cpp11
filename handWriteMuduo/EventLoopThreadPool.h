#pragma once

#include"noncopyable.h"

#include<functional>
#include<string>
#include<vector>
#include<memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop* )>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThread) {numThreads_ = numThread;}

    // 根据numThread在线程池中创建相应个数的事件线程
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // 如果工作在多线程中,baseloop_(相当于mainloop)默认以轮询的方式将channel分配给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const {return started_;}
    const std::string name() const {return name_;}

private:
    EventLoop *baseLoop_;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;      // 做轮询的下标
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};