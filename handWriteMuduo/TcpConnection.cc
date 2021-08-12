#include"TcpConnection.h"
#include"Logger.h"
#include"Socket.h"
#include"Channel.h"
#include"EventLoop.h"

#include<functional>
#include<errno.h>
#include<memory>
#include<unistd.h>
#include<strings.h>

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/tcp.h>


static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if(loop ==nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection is null! \n", __FILE__, __FUNCTION__,__LINE__);
    }
    return loop;
    
}

TcpConnection::TcpConnection(EventLoop *loop,
                const std::string &nameArg,
                int sockfd,
                const InetAddress &localAddr,
                const InetAddress &peerAddr)
            :loop_(CheckLoopNotNull(loop))
            ,name_(nameArg)
            ,state_(kConnecting)
            ,reading_(true)
            ,socket_(new Socket(sockfd))
            ,channel_(new Channel(loop, sockfd))
            ,localAddr_(localAddr)
            ,peerAddr_(peerAddr)
            ,highWaterMark_(64*1024*1024)    // 64M
{
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead,this,std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite,this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError,this));

    LOG_INFO("tcpConnection::ctor[%s] at fd=%d\n", name_.c_str(),sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::ctor[%s] at fd=%d state=%d \n", name_.c_str(),channel_->fd(),(int)state_);
}

// 发送数据
void TcpConnection::send(const std::string &buf)
{
    if(state_==kConnected)
    {
        if(loop_->isInLoopThread())     // 判断创建该loop的线程和当前线程是否相等
        {
            sendInLoop(buf.c_str(),buf.size());
        }
        else        // 否则将需要执行的操作sendInLoop作为回调函数注册到此loop中,并唤醒该loop的线程来处理,即下面函数runinloop()
        {
            // bind()能够将用户提供的需要参数的函数调整为不需要参数的函数对象.
            // runInloop(Functor cb)中Functor为无参函数对象,sendInloop(char* data,size_t len)有两个参数
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
    
}

// 发送数据 应用写的快,内核发送数据慢,需要把待发送数据写入缓冲区,而且设置了水位回调
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote =0;      // 已发送  
    size_t remaining =len;  // 待发送
    bool faultError =false;

    if(state_== kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return ;
    }

    // channel第一次开始写数据,而且缓冲区没有待发送的数据
    if(!channel_->isWriting()&& outputBuffer_.readableBytes()==0)
    {
        nwrote =::write(channel_->fd(), data, len);
        if(nwrote>=0)
        {
            remaining = len -nwrote;
            if(remaining==0&& writeCompleteCallback_)   // 数据全部发送完成,不用在给channel设置EPOLLOUT事件了
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            nwrote =0;
            if(errno!= EWOULDBLOCK) //产生除无数据可写的其他错误
            {
                LOG_ERROR(" TcpConnection::sendInLoop");
                if(errno== EPIPE||errno==ECONNRESET)
                {
                    faultError=true;
                }
            }
        }
    }
    /*
    说明当前这一次write并没有把数据全部发送出去,因此将剩余的数据保存到发送缓冲区outpurBuffer中,然后给channel注册epollout事件,
    poller发现tcp发送缓冲区有数据,会通知相应的sockfd(channel),调用writeCallback_回调方法,也就是Tcpconnection::handleWrite()方法,
    把发送缓冲区中的数据全部发送完成
    */
    if(!faultError&& remaining>0)
    {
        // 目前发送缓冲区中剩余待发送数据的长度
        size_t oldlen = outputBuffer_.writableBytes();
        if(oldlen +remaining >= highWaterMark_&& oldlen <highWaterMark_ && highWaterMark_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldlen+remaining));
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if(!channel_->isWriting())
        {
            channel_->enableWriting();  // 这里一定要注册channel的写事件,否则poller不会给channel通知epollout=>handlewrite()
        }
    }
}

void TcpConnection::connectEstablished()
{
    setState(kConnected);
    /*
    channel用一个弱智能指针观察channel对应的TcpConnection的存活状态,将这两个绑定后,tied置为true.后面有事件到来时需要调用相应的回调函数,
    此函数内部会先通过将此弱智能指针提升为强智能指针,成功则此连接TcpConnection存在并执行回调函数,不存在说明此连接已销毁不执行任何操作
    */
    channel_->tie(shared_from_this());
    channel_->enableReading();

    // 新链接建立,执行相应回调:OnconnectionCallback
    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestoryed()
{
    if(state_ ==kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();     //关闭channel
        connectionCallback_(shared_from_this());
    }
    channel_->remove();     // 把channel从对应poller中删除
}

void TcpConnection::shutdown()
{
    if(state_==kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void  TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())
    {
        socket_->shutdownWrite();   // 调用Socket封装的底层::shutdown()函数.  关闭写端后,poller会给channel通知关闭事件,channel调用handleClose()回调函数
    }
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int saveErrno =0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);    // 从fd上读数据存入inputbuffer
    if(n>0)
    {
        // 已连接的用户有可读事件发生了,调用用户传入的回调操作即向TcpServer中注册的OnMessageCallback,  shared_from_this():获取当前对象的智能指针
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if(n==0)   // 断开  
    {
        handleClose();
    }
    else        // 错误
    {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    int saveErrno =0;
    if(channel_->isWriting())
    {
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if(n>0)
        {
            outputBuffer_.retrive(n);            // 清除已发送的数据
            if(outputBuffer_.readableBytes()==0)
            {
                channel_->disableWriting();     // 设置不允许发送
                if(writeCompleteCallback_)
                {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));  // 此fd发送完数据后,让此fd调用此回调函数.
                }
                if(state_ ==kDisconnecting) // 数据执行过程中shutdown()被调用,此连接被设置为关闭
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("Tcpconnection fd =%d id down , nomore writing \n", channel_->fd());
    }
}

void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd =%d state =%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);       // 执行连接关闭的回调,TcpServer向TcpConnection注册的connectionCallback_ , 即用户向TcpServer注册的
    closeCallback_(connPtr);            // 关闭链接的回调TcpServer向TcpConnection注册的closeCallback_,即为TcpServer本身的,不用外部用户注册的TcpServer::removeConnection
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err =0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen)<0)
    {
        err =errno;
    }
    else
    {
        err =optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s  - SO_ERROR:%d \n", name_.c_str(), err);
}


