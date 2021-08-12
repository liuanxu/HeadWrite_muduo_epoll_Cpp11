#include<mymuduo/TcpServer.h>
#include<mymuduo/Logger.h>

#include<string>
#include<functional>

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
    :server_(loop,addr,name)
    ,loop_(loop)
    {
        //
        server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        server_.setThreadNum(3);
    }

    void start()
    {
        server_.start();
    }

private:
    // 连接建立或断开回调
    void onConnection(const TcpConnectionPtr &conn)
    {
        if(conn->connected())
        {
            LOG_INFO("conn up : %s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("conn down : %s", conn->peerAddress().toIpPort().c_str());
        }
    }
    // 可读写消息回调
    void onMessage(const TcpConnectionPtr &conn,Buffer *buf, Timestamp time)
    {
        std::string msg = buf->retriveAllString();
        conn->send(msg);
        conn->shutdown();   // shutdownwrite=>EPOLLHUP =>closeCallback
    }

    EventLoop *loop_;
    TcpServer server_;

};

int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    EchoServer server(&loop, addr, "echoserver-01");    // acceptor =>non-blocking listenfd => create bind
    server.start(); // listen loopthread listenfd =>acceptchannel =>mainloop => mainloop =>newconnection 
    loop.loop();    // 启动mainloop的底层poller

}