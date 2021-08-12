#include "Channel.h"
#include"EventLoop.h"
#include"Logger.h"

#include<sys/epoll.h>
#include<string>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN| EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

// EventLoop : ChannelList (里面存了多个channel) + Poller, 
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), 
    fd_(fd), 
    events_(0), 
    revents_(0), 
    index_(-1), 
    tied_(false){}

Channel::~Channel(){}


void Channel::tie(const std::shared_ptr<void> &obj){
    tie_= obj;      // 弱指针观察强指针
    tied_ = true;
}

// 当改变channel所表示fd的events事件后,update负责在poller里面更改fd相应的事件epool_ctl,向EventLoop通知,由EvenetLoop改变poller.  ChannelList, Poller为EventLoop的两个子组件
void Channel::update(){
    // 通知Channel所属的EventLoop, 调用poller的相应方法,注册fd的events事件
    //add code...
    loop_->updateChannel(this);

}

// 在channel所属的eventloop中，把当前的channel删除掉
void Channel::remove(){
    //add code... 
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime){
    if(tied_)
    {  
        std::shared_ptr<void> guard = tie_.lock();  // 弱智能指针存在,提升为强智能指针
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else 
    {
        handleEventWithGuard(receiveTime);
    }
}

//  根据poller通知的channel发生的具体事件, 调用具体的回调函数
void Channel::handleEventWithGuard(Timestamp receiveTime){

    LOG_INFO("channel handleEvent revents :%c\n", revents_);

    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))      // 出问题了,发生异常
    {
        if(closeCallback_)
        {
            closeCallback_();
        }
    }
    
    if((revents_ & EPOLLERR)){          // 发生错误
        if(errorCallBack_){
            errorCallBack_();
        }
    }

    if(revents_ &(EPOLLIN |EPOLLPRI)){
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }

    if(revents_ & EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }
}


