#include "TcpServer.h"
#include"Logger.h"
#include"TcpConnection.h"

#include<strings.h>
#include<functional>


static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if(loop ==nullptr)
    {
        LOG_FATAL("%s:%s:%d mainloop is null! \n", __FILE__, __FUNCTION__,__LINE__);
    }
    return loop;
    
}

TcpServer::TcpServer(EventLoop *loop,
                    const InetAddress &listenAddr,
                    const std::string &nameArg,
                    Option option )
                    :loop_(CheckLoopNotNull(loop))
                    ,ipPort_(listenAddr.toIpPort())
                    ,name_(nameArg)
                    ,acceptor_(new Acceptor(loop, listenAddr, option ==kReusePort))
                    ,threadPool_(new EventLoopThreadPool(loop, name_))
                    ,connectionCallback_()
                    ,messageCallback_()
                    ,nextConnId_(1)
                    ,started_(0)
{
    // 向acceptor注册的回调函数,有新链接到来时会调用此函数,主要做的事情有:轮询操作选择一个subloop,唤醒subloop,把当前的connfd封装成channel分给subloop
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));

}

 TcpServer::~TcpServer()
 {
     // 销毁过程中会调用函数removeConnectionInLoop(),其内部会调用.erase()将conn..Ptr删除,因此必须先将conn_Ptr取出来,计数-1:item.second.reset();再调用conn->getLoop()->runInLoop().
     for(auto item:connections_)
     {
         TcpConnectionPtr conn(item.second);    // conn为局部变量,出作用域自动销毁,share_ptr计数-1,若为0,则此指针指向的对象资源释放
         item.second.reset();   // 删除Map表中的share_ptr,shared_ptr的reset( )函数的作用是将引用计数减1，停止对指针的共享，除非引用计数为0，否则不会发生删除操作。
         conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestoryed, conn)); // 销毁链接
     }
 }


void TcpServer::setThreadNum(int numThread)
{
    threadPool_->setThreadNum(numThread);
}


//开启服务监听 之后下一步就是开启loop_.loop()
void TcpServer::start()
{
    if(started_++==0)   // 防止一个对象被start多次  
    {
        threadPool_->start(threadInitCallback_);    // 启动底层的loop线程池,此时才会真正创建子loop线程
        loop_->runInLoop(std::bind(&Acceptor::listen,acceptor_.get())); // 唤醒启动mainloop,  并向其注册唤醒之后需要做的回调事件
    }
}

// 有一个新的客户端连接连上来,acceptor会调用此回调
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    EventLoop *ioLoop = threadPool_->getNextLoop();

    char buf[64] ={0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    nextConnId_++;      // 只在mainloop中处理,不涉及线程安全问题
    std::string connName = name_ + buf;

    LOG_INFO("tcpserver::newConnection [%s] - new connection [%s] from %s \n", name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本机ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if(::getsockname(sockfd, (sockaddr*) &local, &addrlen)<0)
    {
        LOG_ERROR("sockets::getlocalAddr");
    }

    InetAddress localAddr(local);
    // 根据链接成功的sockfd,创建Tcpconnection连接对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr,peerAddr));
    connections_[connName] =conn;

    // connectionCallback_是用户设置给TcpServer,而本函数是Tcpserver注册给TcpConnection的,
    // 用户=>Tcpserver=>TcpConnection=>channel=>poller=>notify channel 调用回调函数
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // 设置了如何关闭链接的回调
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));



}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] -connection %s \n", name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestoryed, conn));

}


