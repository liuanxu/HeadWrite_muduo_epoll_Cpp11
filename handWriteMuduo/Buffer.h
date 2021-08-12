#pragma once


#include<vector>
#include<string>

// 网络库底层的缓冲区类型定义
class Buffer
{
public:
    static const size_t kCheapPrepend =8;
    static const size_t kInitialSize =1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        :buffer_(kCheapPrepend + initialSize)
        ,readerIndex_(kCheapPrepend)
        ,writerIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const
    {
        return writerIndex_- readerIndex_;
    }

    size_t writableBytes() const
    {
        return buffer_.size() -writerIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    // 返回缓冲区中可读数据的起始地址
    const char* peek()const
    {
        return begin() +readerIndex_;
    }

    // onmessage  string<-buffer
    void retrive(size_t len)
    {
        if(len <readableBytes())
        {
            readerIndex_ +=len;     // 只读取了部分数据
        }
        else
        {
            retriveAll();       // 
        }
    }

    void retriveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    // 把onmessage函数上报的buffer数据,转成string类型的数据返回
    std::string retriveAllString()
    {
        return retriveAsString(readableBytes());    // 
    }

    std::string retriveAsString(size_t len)
    {
        std::string result(peek(), len);    // 将缓冲区中的可读数据取出成string
        retrive(len);       // 读出数据后,对已读buffer复位
        return result;

    }

    // buffer.size() -writeIndex_可写的buffer长度      len需要写的数据长度
    void ensureWritableBytes(size_t len)
    {
        if(writableBytes() <len)
        {
            makeSpace(len);     // 扩容函数
        }
    }

    // 把[data, data + len]内存上的数据添加到writable缓冲区当中
    void append(const char *data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ +=len;
    }

    char* beginWrite()
    {
        return begin() +writerIndex_;
    }

    const char* beginWrite() const
    {
        return begin() +writerIndex_;
    }

    // 从fd上读取数据到缓冲区
    ssize_t readFd(int fd, int *saveErrno);
    //通过fd发送数据
    ssize_t writeFd(int fd, int *saveErrno);

private:
    char* begin()
    {
        // it.operator*()取出迭代器的首元素
        return &*buffer_.begin();   
    }

    const char* begin()const
    {
        // it.operator*()取出迭代器的首元素
        return &*buffer_.begin();   
    }

    void makeSpace(size_t len)
    {
        // 当只读取部分数据时,readIndex_会向后移动大于 kCheapPrepend ,因此 prependableBytes() 读出的长度可能大于kCheapPrepend
        if(writableBytes() +prependableBytes() <len +kCheapPrepend)
        {
            buffer_.resize(writerIndex_ +len);
        }
        else
        {
            // move readable data to the front readerable buffer, to make behind space size for the len
            size_t readable = readableBytes();
            std::copy(begin() +readerIndex_,
                        begin()+writerIndex_,
                        begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }



    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;

};