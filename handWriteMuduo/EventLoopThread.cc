#include"EventLoopThread.h"
#include"EventLoop.h"


EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    :loop_(nullptr)
    ,exiting_(false)
    ,thread_(std::bind(&EventLoopThread::threadFunc,this), name)
    ,mutex_()
    ,cond_()
    ,callback_(cb)
{

}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if(loop_!=nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop()
{
    thread_.start();    // 启动底层的线程,start()=>func_()

    EventLoop *loop =nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_==nullptr)
        {
            cond_.wait(lock);       // 等待子线程创建成功,使得loop_指向生成EventLoop对象,之后被唤醒
        }
        loop = loop_;       // 得到生成的loop
    }
    return loop;
}

// 这个函数是在新线程里面运行的, 
void EventLoopThread::threadFunc()
{
    EventLoop loop;     // 创建一个独立的Eventloop,和这个新线程一一对应的, one loop per thread
    if(callback_)       // 如果对这个线程定义了回调函数,则调用
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;          // 使主线程中对象的loop指针指向子线程中的loop对象,
        cond_.notify_one();     // 唤醒上面主线程中startloop()函数中的等待
    }
    loop.loop();    // EventLoop loop() =>Poller.poll()=>epoll_wait(),loop()不退出则子线程一直在loop()函数内运行

    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;    // quit loop
}
