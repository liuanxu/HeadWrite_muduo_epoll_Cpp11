# pragma once

#include<functional>
#include<vector>
#include<atomic>
#include<memory>
#include<mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include"CurrentThread.h"



class Channel;
class Poller;

//事件循环类reactor主要包含了两大模块 Channel(相当于Event结构体,其中包含Fd,感兴趣的事件..)  Poller(epoll的抽象)
class EventLoop :noncopyable
{
public:
    using Functor = std::function<void()>;  // 没有返回值的函数

    EventLoop();
    ~EventLoop();

    void loop();    // 开启事件循环
    void quit();

    Timestamp pollReturnTime() const {return pollReturnTime_;}

    void runInLoop(Functor cb);     // 在当前loop中执行cb,
    void queueInLoop(Functor cb);   // 把cb放入队列中,唤醒loop所在的线程,执行cb

    void wakeup(); // mainreactor唤醒subreactor

    void updateChannel(Channel * channel);       // 用来提供给Channel调用,Channel:update=>eventloop:update=>poller:update(epoll_ctrl()),来改变Channel感兴趣的事件
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    bool isInLoopThread() const {return threadId_ == CurrentThread::tid();} // 创建此loop的线程,判断创建该loop的线程和当前线程是否相等

private:
    void handleRead();  // wake up
    void doPendingFunctors();   // 执行回调函数

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;  // 原子操作,通过CAS实现
    std::atomic_bool quit_;     // 标识推出loop循环
    
    const pid_t threadId_;      // 记录当前loop所在线程的id,创建此loop的线程
    Timestamp pollReturnTime_;  // poller返回发生事件的channels的时间点

    std::unique_ptr<Poller> poller_;    // 此eventloop管理的poller

    // wakeuupFd使用内核提供的系统api函数eventfd()创建,主要是用于进程或者线程间的通信(如通知/等待机制的实现)。
    // int eventfd(unsigned int initval, int flags);eventfd()创建一个“eventfd对象”，这个对象能被用户空间应用用作一个事件等待/响应机制，靠内核去响应用户空间应用事件,也就是内核会notify用户空间的一个应用程序.
    // 在此处, 多reactor模式中,当新用户到来时,mainreactor会通过轮询操作将新的fd分配给subreactor.而当没有事件发生时subreactor是阻塞的,等待被唤醒.
    int wakeupFd_;      // 此变量作用就是用作mainreactor唤醒subreactor
    std::unique_ptr<Channel> wakeupChannel_;    // 将上面的wakeupfd及其感兴趣的事件打包成channel

    ChannelList activeChannels_;
    // Channel * currentActiveChannel_;

    std::atomic_bool callingPendingFunctors_;   // 标识当前loop是否有需要执行的回调操作, 
    std::vector<Functor> pendingFunctors_;      // 存储loop需要执行的所有的回调操作,
    std::mutex mutex_;      // 互斥锁,用来保护上面vector容器的线程安全操作

};