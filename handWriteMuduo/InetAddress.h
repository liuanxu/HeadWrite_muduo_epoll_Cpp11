#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>     // 包含sockaddr_in此结构体的头文件
#include <iostream>


// 封装socket地址类型
class InetAddress{
public:
    explicit InetAddress(uint16_t port =0, std::string ip = "127.0.0.1");  // explicit:禁止隐式对象转换
    explicit InetAddress(sockaddr_in &addr) :addr_(addr) {}

    // 获取ip地址,端口号port 
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const {return &addr_;}     // 获取成员变量
    void setSockAddr(const sockaddr_in &addr)   {addr_ = addr;}

private:
    sockaddr_in addr_;
};