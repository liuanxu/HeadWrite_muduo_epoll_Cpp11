#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

/*
Channel理解为通道, 封装了sockfd和其感兴趣的event, 如EPOLLIN,EPOLLOUT事件, 还绑定了poller返回的具体事件
*/

class EventLoop;    // 类的前置声明,在源文件中再包含该类具体的头文件,减少头文件嵌套
class Timestamp;

class Channel:noncopyable{
public:
    using EventCallback = std::function<void()> ;
    using ReadEvenCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到poller通知后,调用相应的回调方法,即对private：中定义的四个回调函数对象进行调用
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象,提供四个设置函数，供外部调用进行回调函数的注册!!!
    void setReadCallback(ReadEvenCallback cb)   {readCallback_ = std::move(cb);}        // 左值转换成右值，减少拷贝操作
    void setWriteCallback(EventCallback cb) {writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb) {closeCallback_ = std::move(cb);}
    void setErrorCallback(EventCallback cb) {errorCallBack_ = std::move(cb);}

    // 防止当channel被手动remove掉, channel还在执行回调操作,使用弱智能指针观察channel是否被删除
    void tie(const std::shared_ptr<void>&);

    int fd() const {return fd_;}
    int events() const {return events_;}

    // 返回的事件是由epoll进行监听的，channel并不知道返回的具体事件，因此需要对外提供一个接口来设置revents
    void set_revent(int revt) {revents_ = revt;}    

    // 设置fd相应的事件状态,将fd感兴趣事件events相应的事件类型位置1,update()是调用epoll_ctrl/epoll_modify等
    void enableReading() {events_ |= kReadEvent; update();}
    void disableReading() {events_ &= ~kReadEvent; update();}
    void enableWriting() {events_ |= kWriteEvent; update();}
    void disableWriting() {events_ &= ~kWriteEvent; update();}
    void disableAll()   {events_ = kNoneEvent; update();}

    // 对当前感兴趣事件状态进行判断
    bool isNoneEvent() const {return  events_ == kNoneEvent;}
    bool isWriting() const {return  events_ & kWriteEvent;}
    bool isReading() const {return events_ & kReadEvent;}

    int index() {return index_;}
    void set_index(int idx)    {index_ = idx;}

    // 返回当前channel所属的eventloop,一个线程对应一个eventloop，一个eventloop包含一个epoll+多个channel!!!
    EventLoop* ownerLoop() {return loop_;}
    void remove();



private:

    // update()是调用epoll_ctrl/epoll_modify等
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    // 对感兴趣状态事件的描述
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;   // 事件循环, 
    const int fd_;      // fd,Poller监听的对象, 
    int events_;        // 注册fd感兴趣的事件, 
    int revents_;       // poller返回的具体发生的事件
    int index_;

    /*
    用一个弱智能指针观察channel对应的TcpConnection,将这两个绑定后,tied置为true.后面有事件到来时需要调用相应的回调函数,
    此函数内部会先通过将此弱智能指针提升为强智能指针,成功则此连接TcpConnection存在并执行回调函数,不存在说明此连接已销毁不执行任何操作
    */
    std::weak_ptr<void> tie_;   // 
    bool tied_; 

    // 因为channel通道能够获知fd最终发生的具体事件,revents对应的事件回调函数,所以他负责调用具体事件的回调操作,但是回调函数具体处理哪些业务，需要对channel进行注册
    ReadEvenCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallBack_;

};