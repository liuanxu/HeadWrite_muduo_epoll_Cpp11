#pragma once

#include "noncopyable.h"
#include"Socket.h"
#include"Channel.h"


class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb)  {newConnectionCallback_ =cb;}

    bool listenning() const {return listenning_;}
    void listen();


private:
    void handleRead();

    EventLoop *loop_;   // acceptor所属于的Eventloop,也就是用户定义的baseloop(也就是mainreactor的eventloop)
    Socket acceptSocket_;    // 定义的为变量对象,需要包含相应的头文件(如果是指针则不用,但需要前置声明:class name;)
    Channel acceptChannel_; //此acceptChannel_用来封装上面的acceptSocket_
    /*
    此回调函数作用:当新的连接到来时主eventlooo需要唤醒一个subloop,并将需要将新的fd分配给subloop,也就是唤醒后会执行
    mainloop向subloop注册的回调函数(此函数),此函数作用就是将新的fd及其相关的InetAddress等信息封装成channel,并将此channel加入到唤醒的Eventloop中
    */
    NewConnectionCallback newConnectionCallback_;   
    bool listenning_;
};