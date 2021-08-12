#include"Socket.h"
#include"Logger.h"
#include"InetAddress.h"

#include<unistd.h>
#include<strings.h>

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/tcp.h>

Socket::~Socket()
{
    close(sockfd_);
}

/*bind():
将address指向的sockaddr结构体中描述的一些属性（IP地址、端口号、地址簇）与socket套接字绑定，也叫给套接字命名。

调用bind()后，就为socket套接字关联了一个相应的地址与端口号，即发送到地址值该端口的数据可通过socket读取和使用。当然也可通过该socket发送数据到指定目的。

对于Server，bind()是必须要做的事情，服务器启动时需要绑定指定的端口来提供服务（以便于客户向指定的端口发送请求），
对于服务器socket绑定地址，一般而言将IP地址赋值为INADDR_ANY（该宏值为0），即无论发送到系统中的哪个IP地址（当服务器有多张网卡时会有多个IP地址）的请求都采用该socket来处理，而无需指定固定IP。

对于Client，一般而言无需主动调用bind()，一切由操作系统来完成。在发送数据前，操作系统会为套接字随机分配一个可用的端口，同时将该套接字和本地地址信息绑定。
*/
void Socket::bindAddress(const InetAddress &localaddr)
{
    if(0!=::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(),sizeof(sockaddr_in)))
    {
        LOG_FATAL("bind sockfd: %d fail \n", sockfd_);      // 绑定失败
    }
}

/*listen(): listen函数的作用是把一个未连接的套接字转换为被动套接字，指示内核应该接受指向该套接字的连接请求。使套接字从CLOSED状态转换到LISTEN状态。
listen函数把一个未连接的套接字转换成一个被动套接字，接受来自其他主动套接字的连接请求。
1024:通过connect函数来链接服务socket，并正处于等待服务socket accept的客户socket个数
*/
void Socket::listen()
{
    if(0!= ::listen(sockfd_, 1024))
    {
        LOG_FATAL("listen sockfd:%d \n", sockfd_);      // 监听失败
    }
}

/*
accept()
原型： int accept (int sockfd, struct sockaddr *addr, socklen_t *addrlen)

功能描述：accept()函数仅被TCP类型的服务器程序调用，从已完成连接队列返回下一个建立成功的连接，如果已完成连接队列为空，线程进入阻塞态睡眠状态。成功时返回套接字描述符，错误时返回-1。

如果accpet()执行成功，返回由内核自动生成的一个全新socket描述符，用它引用与客户端的TCP连接。通常我们把accept()第一个参数成为
监听套接字（listening socket），把accept()功能返回值成为已连接套接字（connected socket）。一个服务器通常只有1个监听套接字，
监听客户端的连接请求；服务器内核为每一个客户端的TCP连接维护1个已连接套接字，用它实现数据双向通信。
*/
int Socket::accept(InetAddress *peeraddr )
{
    /* BUG solve
    运行过程中当有新链接到来时,报错:1.EPOLLER::poll() err! 2./home/liuanxu/Documents/handWriteMuduo/Acceptor.cc:handleRead:77 accept err:22 ,
    (1)首先第一次尝试用gdb调试进入到第一个错误提示的epollpoller的poll()函数也就是对epoll_wait()的封装这个函数,发现产生的错误信息打印原因为中断系统调用错误, 
    之后检查epoll_wait()函数返回的numEvents为-1,errorno为4验证了确实是系统调用产生了错误,但是不知道具体错误的位置.(2)然后又看到日志打印确实有新的事件发生,
    并且查阅第二个错误在自己封装的accept()函数处,查阅errorno:22含义(perror 22):OS error code  22:  Invalid argument 无效参数,
    因此去查询accept()函数的输入参数,进入到调用的系统底层函数::accept,之后检查后发现输入参数类型都没有问题.又检查自己封装的accept()其他代码没问题,
    然后重新编译调试发现日志打印显示可以正常检测到newconnect的到来且错误号为0没有错误发生,但是不能显示系统accept()之后的日志打印.
    最后又定位在系统accept()函数上,之后又查阅accept()函数的帮助文档发现是addrlen问题,    它是一个值结果参数，
    调用函数必须初始化为包含addr所指向结构大小的数值，函数返回时包含对等地址(一般为服务器地址)的实际数值,对比代码发现没有对addrlen初始化为前一个sockaddr的大小,
    */
    sockaddr_in addr;
    socklen_t len = sizeof addr;
    bzero(&addr, sizeof addr);
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len,SOCK_NONBLOCK|SOCK_CLOEXEC); // 最后一个参数是将connfd设置为非阻塞模式
    if(connfd >=0)
    {
        peeraddr->setSockAddr(addr);    // addr保存的是请求连接客户端的地址,通过此函数的参数peeeraddr传出去
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    if(::shutdown(sockfd_, SHUT_WR)<0)
    {
        LOG_ERROR("shutdownwrite error!");
    }
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on? 1:0;
    ::setsockopt(sockfd_, IPPROTO_TCP,TCP_NODELAY, &optval, sizeof optval);
}

void Socket::setReuseAddr(bool on)
{
    int optval = on? 1:0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

void Socket::setReusePort(bool on)
{
    int optval = on? 1:0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on)
{
    int optval = on? 1:0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}