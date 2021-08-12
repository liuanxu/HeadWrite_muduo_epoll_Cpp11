#pragma once

#include"Poller.h"
#include"Timestamp.h"

#include<vector>
#include<sys/epoll.h>

class Channel;
/**
 * epoll_create()
 * epoll_ctl()  add/mod/del
 * epoll_wait()
 * 
 **/
class EPollPoller:public Poller{
public:
    EPollPoller(EventLoop *loop);       // epoll_create()
    ~EPollPoller() override;

    //  重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList * activeChannels) override;       // epoll_wait()
    void updateChannel(Channel *channel) override;      // add/mod
    void removeChannel(Channel *channel) override;      //del

private:
    static const int kIniteventListSize =16;    // 定义EventList的初始长度 16

    // 填写活跃的连接, 
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新Channel通道,更新channel感兴趣的事件,epoll_ctrl(),由updateChannel,removeChannel调用
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;     // 作为参数传入epoll_wait（），存储发生事件的fd

    int epollfd_;   // 由构造函数中epoll_create()创建,析构函数中删除
    EventList events_;
};