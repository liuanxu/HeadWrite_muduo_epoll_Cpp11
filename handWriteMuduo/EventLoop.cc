#include "EventLoop.h"
#include"Logger.h"
#include"Poller.h"
#include"Channel.h"


#include<sys/eventfd.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<memory>

// 防止一个线程创建多个Eventloop
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的poller IO复用接口的超时时间
const int kPollTimeMs =10000;

// 创建wakeupfd, 用来notify唤醒subbreactor处理新来的channel
int createEventfd()
{
    int evfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evfd< 0)
    {
        LOG_FATAL("eventfd error: %d \n", errno);
    }
    return evfd;
}

EventLoop::EventLoop()
    :looping_(false),
    quit_(false),
    callingPendingFunctors_(false),
    threadId_(CurrentThread::tid()),            // 创建loop的线程id
    poller_(Poller::newDefaultPoller(this)),    // 由智能指针进行控制uniqueptr<Poller>,该函数参数需要传入EventLoop指针,而当前对象即为EventLoop,即传入this指针 
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_)) // 由智能指针进行控制uniqueptr<Channel>,自动析构
    // currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if(t_loopInThisThread)
    {
        LOG_FATAL("another eventloop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置wakeupfd的事件类型一iji发生事件后的回调函数
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 设置wakeupfd的事件类型,发生事件后的回调函数每一个eventloop都将监听wakeupchannel的EPOLLIN读事件,其内部调用了Channel::update()=>eventloop::uodate()=>poller::update()
    wakeupChannel_->enableReading();

}

EventLoop::~EventLoop()
{   
    // 关闭wakeupfd之前需要先关闭其对应的Channel
    wakeupChannel_->disableAll();   // 设置成所有事件都不感兴趣,
    wakeupChannel_->remove();       // 将wakeupchannel从此eventloop中删除
    ::close(wakeupFd_);
    t_loopInThisThread =nullptr;

}

void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("Eventloop %p start looping \n", this);

    while(!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd  一种是client的fd,一种是wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for(Channel *channel :activeChannels_)
        {
            // poller监听了哪些channel发生了事件,然后iou上报给EventLoop,之后EventLoop处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }

        /*
        执行当前eventloop事件循环需要处理的回调函数,主mainloop在accept 新fd后需要将fd打包成channel将channel加入到eventloop中,因此在需要唤醒subloop将新fd分发配给subloop后还要执行下面的函数
        mainloop事先注册一个回调cb(需要subloop来执行)  wakeup subloop之后,执行doPendingFunctors,里面执行之前mainloop注册的回调函数
        */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

// 退出事件循环,1.loop在自己的线程中调用quit()  2.在非此lopp的线程中,调用此loop的quit()
void EventLoop::quit()
{
    quit_ =true;

    // 如果是在其他线程中调用quit(),: 在一个subloop(worker线程),调用了mainloop(IO线程)的quit()
    if(!isInLoopThread())
    {
        wakeup();
    }
}

// 在当前loop中执行cb,
void EventLoop::runInLoop(Functor cb)
{
    if(isInLoopThread())        // 在当前的loop线程中,执行cb   
    {
        cb();
    }
    else                        // 在非当前loop线程中执行cb,就需要唤醒loop所在的线程,执行cb
    {
        queueInLoop(cb);
    }
}   

// 把cb放入队列中,唤醒loop所在的线程,执行cb,在其他loop线程中执行cb
void EventLoop::queueInLoop(Functor cb)
{
    // 可能涉及多个loop调用同一个loop的runinloop()执行回调,就是多个loop往一个loop的pendingfunctors(vector)中添加cb
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    //唤醒相应的,需要执行上面回调操作的loop的线程
    /*
    后面一种情况callingPendingFunctors_表示此loop在其线程中执行,但是正在执行回调函数.如果不处理这种情况的话,当执行完此轮回调函数后,
    loop就进入poller->poll()函数中处于休眠状态,则其他loop向此loop中存入的cb就得不到执行.因此此种情况也需要wakeup(),
    即向wakeupfd写入事件,当此loop执行的此轮回调函数执行完毕后,回到poller->poll()后即会被wakeupfd唤醒,继续执行回调.
    */   
    if(!isInLoopThread()||callingPendingFunctors_)  // callingPendingFunctors_
    {
        wakeup();   // 唤醒loop所在线程 
    }

}  

void EventLoop::handleRead()
{
    uint64_t one =1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if(n!=sizeof one)
    {
        LOG_ERROR("Eventloop::handlread() reads %ld bytes instead of 8", n);
    }
}

// 用来唤醒loop所在的线程, 向wakeupfd写数据,wakeupChannel就会发生读事件,当前loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one =1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if(n!= sizeof one)
    {
        LOG_ERROR("EventLoop: wakeup() write %lu bytes instead of 8 \n", n);
    }
}

// 用来提供给Channel调用,Channel:update=>eventloop:update=>poller:update(epoll_ctrl()),来改变Channel感兴趣的事件
void EventLoop::updateChannel(Channel * channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

// 最终执行回调操作的地方
void EventLoop::doPendingFunctors()
{
    /*
    定义一个局部的vector<>functors, 之后将局部的functors与全局的vector<>pendingfunctors_交换,
    pendingfunctors_所有cb存入functors,此时为空.这样避免一个一个取出时频繁调用lock,能够提高效率.
    降低了从pendingfunctors_取cb时有其他loop向pendingfunctors_添加cb的冲突机率.
    */
   std::vector<Functor> functors;
   callingPendingFunctors_ = true;
   {
       std::unique_lock<std::mutex> lock(mutex_);
       functors.swap(pendingFunctors_);
   } 

   for(const Functor & functor : functors)
   {
       functor();   // 执行当前loop需要执行的回调操作
   }
   callingPendingFunctors_ = false;
}