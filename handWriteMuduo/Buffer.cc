#include"Buffer.h"

#include<errno.h>
#include<sys/uio.h>
#include<unistd.h>

/* 从fd上读取数据到缓冲区
Buffer缓冲区是有大小的,但是从fd上读数据的时候,不知道itcp数据最终的大小
*/
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65535] = {0};     // 栈上的内存空间   64K

    struct iovec vec[2];            // 定义存储多个缓冲区的数组,vec[0]是new出来的堆上的空间较宝贵,vec[1]是局部的栈上的空间出作用域自动回收

    const size_t writable = writableBytes();    // Buffer底层缓冲区可写空间大小
    vec[0].iov_base = begin() +writerIndex_;
    vec[0].iov_len = writable;

    // Buffer 用完后,向栈空间写数据
    vec[1].iov_base =extrabuf;
    vec[1].iov_len =sizeof extrabuf;

    const int iovcnt = (writable<sizeof extrabuf)? 2:1;     //定义缓冲区数量,意思一次最多读取64k数据
    
    // readv()读取数据,首先最多使用的缓冲区数量为iovcnt,然后读数据先存入第一个缓冲区,地一个放满后再放入第二个
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if(n<0)     // 读取的数据小于0说明出错了
    {
        *saveErrno =errno;
    }
    else if(n<=writable)
    {
        writerIndex_+=n;
    }
    else        // extrabuffer也写入了数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n -writable);      //  writerIndex_开始写 n -writable)大小的数据
    }
    return n;

}

ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = ::write(fd, peek(),readableBytes());
    if(n<= 0)
    {
        *saveErrno =errno;
    }
    return n;
}