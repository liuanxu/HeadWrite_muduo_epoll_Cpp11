#pragma once

#include"noncopyable.h"

#include<functional>
#include<thread>
#include<memory>
#include<unistd.h>
#include<string>
#include<atomic>

class Thread:noncopyable
{
public:
    using ThreadFunc = std::function<void()>;   // 

    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const {return started_;}
    pid_t tid() const {return tid_;}
    const std::string &name() const {return name_;}

    static int numCreated() {return numCreated_;}

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;   // 若直接用thread定义一个对象thread thread_;则意味着此线程直接启动,因此此处用智能指针先定义不启动,由start()函数控制启动,创建真正的线程对象
    pid_t tid_;     // 
    ThreadFunc func_;   // 存储线程函数
    std::string name_;
    static std::atomic_int numCreated_; // 记录产生的线程数量


};