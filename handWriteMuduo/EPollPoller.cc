#include "EPollPoller.h"
#include"Logger.h"
#include"Channel.h"

#include<errno.h>
#include<memory>
#include<unistd.h>
#include<cstring>

// channel未添加到poller中  
const int kNew =-1;     //  channel的成员index =-1
// 已添加  
const int kAdded = 1;
// 已删除
const int kDeleted =2;

EPollPoller::EPollPoller(EventLoop * loop):Poller(loop),epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kIniteventListSize){
    if(epollfd_<0){
        LOG_FATAL("epoll_create error: %d \n", errno);      // epoll_create()创建epollfd_失败，错误，结束运行
    }
}

EPollPoller::~EPollPoller(){
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels){
    LOG_INFO("func=%s => fd total couunt:%lu\n", __FUNCTION__, channels_.size());    // channels_:channelMap:<fd, channel>

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;  // errno为全局的变量,一个eventloop是一个线程, 一个线程包含一个poller.在运行时可能会有多个线程同时使用errno,因此需要定义局部变量来保存自己线程对应的当前状态
    Timestamp now(Timestamp::now());

    // LOG_INFO("numEvents : %d \n", numEvents);
    // LOG_INFO("saveErrno :%d \n", saveErrno);

    if(numEvents >0)
    {
        LOG_INFO("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, activeChannels);      // activeChannels 为外部传进来的空间,
        if(numEvents== events_.size())                      // 说明其初始化定义的vecctor长度可能不够用了,需要扩容
        {
            events_.resize(events_.size()*2);
        }
    }
    else if(numEvents==0)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else 
    {
        // EINTR为外部中断,如果等于则继续执行epoll_wait(),不等于则发生了错误,进行处理
        if(saveErrno ==EINTR)
        {
            errno = saveErrno;  // erron中间可能被其他eventloop改变
            LOG_ERROR("EPOLLER::poll() err!");
        }
    }
    return now;
}

/*
            eventloop
    channellist     poller
                    channelMap <fd, channel*> epollfd
*/
// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
void EPollPoller::updateChannel(Channel * channel){
    const int index = channel->index();  // 获取channel当前的状态是knew/kadded/kdelete,得出当前channel是否已添加到相应的poller中
    LOG_INFO("func=%s => fd=%d  events =%d index =%d \n",__FUNCTION__,channel->fd(), channel->events(), index);     // __FUNCTION__内部提供的宏定义

    if(index== kNew || index ==kDeleted){
        if(index == kNew)       // 如果是新建的channel，则将其存入到poller的unordered_map中channels_
        {
            int fd = channel->fd();
            channels_[fd] = channel;        
        }
        channel->set_index(kAdded);         // 添加后修改此channel的相应状态
        update(EPOLL_CTL_ADD, channel);     // 添加后向epoll注册此channel
    }
    else        // 此channel已经在poller上注册过了，那么要么修改要么删除
    {
        int fd = channel->fd();
        if(channel->isNoneEvent())          // 说明对所有事件不感兴趣，则删除
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }

}

void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d\n",__FUNCTION__,fd);     // __FUNCTION__内部提供的宏定义


    int index = channel->index();       //  获取此channel在poller中的状态
    if(index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);           // 将此channel设置为未在poller中注册过
}

// 填写活跃的连接, 
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for(int i=0;i<numEvents;i++)
    {
        Channel * channel = static_cast<Channel*>(events_[i].data.ptr); // ptr在uodate()函数中调用epoll_ctrl()前设置为fd对应的channel,现在取出来填充到activechannel中
        channel->set_revent(events_[i].events);     // 将上一步定义的channel中的初始化事件event设置为当前发生的事件
        activeChannels->push_back(channel);
    }
}


// 更新Channel通道,更新channel感兴趣的事件,epoll_ctrl(),由updateChannel,removeChannel调用    : add/mod/del
/*  man epoll_wait()
struct epoll_event 
{
    uint32_t     events;    // Epoll events 
    epoll_data_t data;      // User data variable 
};
typedef union epoll_data {
               void    *ptr;        // 用来存放fd携带的数据，即可存放channel，将两者绑定
               int      fd;
               uint32_t u32;
               uint64_t u64;
           } epoll_data_t;
*/
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    memset(&event, 0, sizeof event);
    int fd = channel->fd();
    
    event.data.fd = fd;
    event.events = channel->events();
    event.data.ptr = channel;


    if(::epoll_ctl(epollfd_, operation, fd, &event)<0)
    {
        if(operation == EPOLL_CTL_DEL)      // 只是删除错误的话，程序还可以继续往下执行，
        {
            LOG_ERROR("epoll_ctl del error: %d\n", errno );
        }
        else        // 如果是add/mod错误的话，则channel获取不到数据程序无法继续执行，涉及到错误力度的大小
        {
            LOG_FATAL("epoll_ctl add/mod error: %d\n", errno);
        }
    }
}