#include"Acceptor.h"
#include"Logger.h"
#include"InetAddress.h"

#include<sys/socket.h>
#include<sys/types.h>
#include<errno.h>
#include<unistd.h>

// 创建listenfd,非阻塞的fd,单个线程(只有一个mainloop)一般用非阻塞的,防止一个connfd阻塞导致线程阻塞无法处理其他的fd,专门一个线程用来处理IO(一个mainloop+eventloopThreadPool)一般用阻塞的.
static int createNonblocking()
{
    int sockfd = ::socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK, IPPROTO_TCP);
    if(sockfd<0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__,__FUNCTION__,__LINE__,errno); // 打印出错的文件=>函数=>行号
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    :loop_(loop)
    ,acceptSocket_(createNonblocking())
    ,acceptChannel_(loop, acceptSocket_.fd())
    ,listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);
    // TcpServer::start()=>Acceptor::=>listen()
    /*
    baseloop开始执行后,需要事先给acceptChannel注册他的回调函数,
    当有新链接(connfd,也就是acceptChannel)到来时需要执行相应的操作来处理这个新的连接,也就是此回调函数
    */
   acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));

}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();    // 设置成对任何事件不感兴趣,关闭此channel
    acceptChannel_.remove();        // 将channel从其所属的loop中删除
}

/*
这里需要注意的是，listen()函数不会阻塞，它主要做的事情为，将该套接字和套接字对应的连接队列长度告诉 Linux 内核，然后，listen()函数就结束。
理解:listen函数不会阻塞，它只是相当于把socket的属性更改为被动连接，可以接收其他进程的连接。listen侦听的过程并不是一直阻塞，
直到有客户端请求连接才会返回，它只是设置好socket的属性之后就会返回。监听的过程实质由操作系统完成。
但是accept会阻塞（也可以设置为非阻塞），如果listen的套接字对应的连接请求队列为空（没有客户端连接请求）,它会一直阻塞等待。
*/
void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();     // listen
    acceptChannel_.enableReading();     // acceptChannel =>poller, 把此channel感兴趣的read事件注册到mainloop管理的poller中,当此channel有事件发生时,就调用上面acceptChannel_.setReadCallback注册的回调函数handleread()

}

// listenfd有事件发生了,就是有新用户链接了
void Acceptor::handleRead()
{
    InetAddress peerAddr;           // 用于传入accept()函数,结束后会改变peerAddr的值,此时值为新链接的地址信息
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd>=0)
    {
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr);   // 链接成功,执行TcpServer给acceptor注册的回调,完成fd=>Channel=>subloop
        }
        else
        {
            ::close(connfd);        // 没有注册回调,则不处理,直接关闭新链接
        }
    }
    else
    {
        LOG_FATAL("%s:%s:%d accept err:%d \n", __FILE__,__FUNCTION__,__LINE__,errno); // 打印出错的文件=>函数=>行号
        if(errno== EMFILE)      // socket资源分配完
        {
            LOG_FATAL("%s:%s:%d sockfd reached limit  \n", __FILE__,__FUNCTION__,__LINE__); // 打印出错的文件=>函数=>行号
        }
    }
    

}