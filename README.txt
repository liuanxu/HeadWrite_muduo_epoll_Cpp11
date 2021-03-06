使用C++11实现基于epoll的muduo库多reactor模型

一个main Reactor负责accept连接，然后把新建立的连接通过轮询方式分配给在某个sub Reactor，该连接的所有操作都在那个sub Reactor所处的线程中完成。

1.Channel对sockfd、感兴趣的事件events、所属的eventloop及其读写事件等回调函数进行了封装,主要负责感兴趣事件的注册与IO事件的响应.

2.Acceptor主要封装了listenfd以及当有新连接到来时的回调函数,此回调函数主要调用accept()函数接收新连接.

3.TcpConnection封装一个连接成功的sockfd对应的Channel、发送和接收缓冲区以及响应发生事件的回调函数.

4.EPollPoller继承于基类Poller,封装了epoll多路I/O事件分发器来监听事件的发生.

5.EventLoop主要对Channels和EPollPoller进行封装,每个线程只能有一个EventLoop对象，负责连接的管理和事件的处理，使用系统函数eventfd()创建的eventfd来唤醒线程.

6.EventLoopThreadPool用于创建IO线程池，用于把新连接TcpConnection分派到某个EventLoop线程上.线程池初始化后开始运行时才会真正创建子线程，通过互斥锁和条件变量实现父线程和子线程之间的通信来判断子线程是否创建成功。

7.TcpServer对Acceptor和EventLoopThreadPool进行封装,用于编写网络服务端，接受客户端的连接。


muduo的线程模型为one loop per thread+thread pool模型，每个线程最多只有一个EventLoop，一个fd只能由一个线程读写， TcpConnection所在的线程由其所属的EventLoop决定，TcpServer支持多线程，有两种模式：

(1)单线程，accept与TcpConnection用同一个线程做IO.

(2)多线程，accept与EventLoop在同一个线程，另外创建一个EventLoopThreadPool，新到的连接会以轮询调度的方式分配到线程池中.主线程负责IO，工作线程负责计算，使用固定大小的线程池，全部的IO工作都在一个Reactor线程完成，而计算任务交给线程池.

C++11相关：
1.使用C++11中memery库中的智能指针替换boost库中的智能指针，如把boost::scoped_ptr替换为std::unique_ptr等。
2.使用c++11中thread库替换linux系统的pthread库。
3.使用C++11的mutex和condition_variable库替换muduo自己封装的互斥量和条件变量。




