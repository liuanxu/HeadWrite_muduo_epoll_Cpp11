#pragma once

#include"noncopyable.h"
#include"Timestamp.h"


#include<vector>
#include<unordered_map>

class Channel;
class EventLoop;

//  muduo库中多路事件分发器的核心IO服用模块
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop * loop);
    virtual ~Poller();

    // 给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList * activeChannels) =0;     //  相当于epoll_wait(), 
    virtual void updateChannel(Channel *channel) =0;        // 相当于epoll_ctl(),给loop进行调用
    virtual void removeChannel(Channel * channel)=0;

    //  判断参数channel是否在当前poller当中
    bool hasChannel(Channel * channel) const;

    // Eventloop 可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop *loop);

protected:
    //  map的key: sockfd   , value: sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop * ownerLoop_;  // 定义poller所属的事件循环eventloop

};