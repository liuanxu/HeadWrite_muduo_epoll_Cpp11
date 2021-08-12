#include "InetAddress.h"

#include<strings.h>     // 包含bzero()函数的头文件, 
#include<string.h>

 InetAddress::InetAddress(uint16_t port, std::string ip ){
    bzero(&addr_, sizeof addr_);       // 申请内存,底部调用的为memset()
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());

 }

// 获取ip地址,端口号port 
std::string InetAddress::toIp() const{
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);     // 将网络字节序转换为本地字节序
    return buf;
}
std::string InetAddress::toIpPort() const{
    //ip:port
    char buf[64] ={0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u", port);        // C 库函数 int sprintf(char *str, const char *format, ...) 发送格式化输出到 str 所指向的字符串。
    return buf;

}
uint16_t InetAddress::toPort() const{
    return ntohs(addr_.sin_port);
}

// #include<iostream>
// int main(){

//     InetAddress addr(8080);
//     std::cout<<addr.toPort()<<std::endl;

//     return 0;
// }
