1.Channel类封装了sockfd,events,revents以及感兴趣事件相对应的回调函数(读,写,关闭,错误),对于上层来说有一个sockfd就会打包成Channel(同时在函数将此Channel保存至感兴趣的事件epoll_event events_.dat.ptr,实际上epoll_event内部保存了fd(event.data.fd), 感兴趣的事件(event.events),channel(event.data.ptr))然后下发到poller上(通过epoll_ctrl()).poller中定义了一个Map:<sockfd, Channel> channels;(可以用来检测eventloop::haschanenl()->poller::haschannel(),removechannel()等),loop()->poll()->epoll_wait()当有事件发生时就能够通过返回的events_.data.ptr找到Channel,然后在loop()中调用Channel->handleEvent()处理事件回调函数.

2.有两种channel,分别由listenfd(acceptorChannel,之后再度包装成为acceptor(包含listen()功能并包含内部已经定义的注册回调函数handleread()(其中又包含::accept()函数处理新链接到来产生的可读事件)))和connfd(普通的Channel,此channel的回调函数由用户注册给TCPserver->Tcpconnection->channel->根据epoll_wait()的返回事件调用响应函数)封装.

3.每个Eventloop中都包含wakeupfd,然后封装成wakeupchanne.此wakeupfd由系统函数::eventfd()创建,创建一个“eventfd对象”，这个对象能被用户空间应用用作一个事件等待/响应机制，靠内核去响应用户空间应用事件,也就是内核会notify用户空间的一个应用程序.直接调用系统函数write()向eventfd中写入数据,这样epoll_wait()就会检测到有可读事件发生,然后函数返回到loop()函数中执行完已注册的fd事件后会接着执行他的std::vector<Functor> pendingFunctors_;中的回调函数(这些函数是此loop在阻塞状态时,新链接到来后acceptor的回调函数向vector<>中添加的新的connfd::established()函数(此函数会调用connfd内channel::enablereading()函数(其内部会调用update()->epoll_ctl())将此channel注册到eventloop内的epollpoller上).

4.一个Tcpconnection对应一个连接成功的客户端,他封装了channel,buffer等,还实现了接受到达的数据存入缓冲区,也可发送数据,最终都是buffer通过调用系统函数::read(),write()来实现.

5.没有做成一个生产者-消费者模型,即mainloop生产链接放入队列中,subloop从同步队列中取链接消费.通过条件变量来进行线程间通信,互斥锁保证线程安全.
