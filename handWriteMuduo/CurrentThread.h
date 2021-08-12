#pragma once

#include<unistd.h>
#include<sys/syscall.h>

namespace CurrentThread
{
    extern __thread int t_cachedTid;

    void cachedTid();

    inline int tid()
    {
        if(__builtin_expect(t_cachedTid==0, 0))  // 没有将次tid放入到缓存到中,主要作用是减少用户态和内核态之间的转换
        {
            cachedTid();
        }
        return t_cachedTid;
    }
}
