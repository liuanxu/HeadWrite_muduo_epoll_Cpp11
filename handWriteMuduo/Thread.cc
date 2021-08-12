#include"Thread.h"
#include"CurrentThread.h"

#include<semaphore.h>


std::atomic_int Thread::numCreated_ (0);

Thread::Thread(ThreadFunc func, const std::string &name )
    :started_(false)
    ,joined_(false)
    ,tid_(0)
    ,func_(std::move(func))
    ,name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if(started_ &&!joined_)
    {
        thread_->detach();  // thread为智能指针,设置成守护线程,当主线程结束此线程自动销毁 
    }
}

// 一个thread对象记录的就是一个新线程的详细信息,!!!!知识点:如何判断子线程是否创建成功
void Thread::start()
{
    started_ = true;
    //定义一个信号量用于线程间的通信
    sem_t sem;
    sem_init(&sem, false, 0);   // 信号量初始化为0

    // 开启线程,[&]表示此lambda函数可以以引用的方式访问当前的thread对象变量和方法.
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        tid_ = CurrentThread::tid();    // 获取子线程id
        sem_post(&sem);         // 信号量加1

        func_();                // 开启一个新线程
    }));      // 此处子线程就已经与主线程分开执行,主线程继续向下执行

    // 这里主线程在此处等待子线程改变信号量,信号量改变之后就说明子线程创建成功,已经进入线程函数内部执行了
    sem_wait(&sem);
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}


void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if(name_.empty())
    {
        char buf[32] ={0};
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}