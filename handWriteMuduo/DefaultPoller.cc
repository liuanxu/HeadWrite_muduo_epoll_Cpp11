#include "Poller.h"
#include"EPollPoller.h"

#include <stdlib.h>

// 属于公共源文件,在此文件中生成具体的实例对象，需要包含epoll或poll头文件，返回一个相应的实例对象
Poller* Poller::newDefaultPoller(EventLoop * loop){
    if(::getenv("MUDUO_USE_POLL"))      // 获取环境变量
    {
        return nullptr; // generate poll
    }
    else 
    {
        return new EPollPoller(loop); //generate epoll
    }
}