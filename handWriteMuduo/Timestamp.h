#pragma once

#include <iostream>
#include <string>

class Timestamp{
public:
    Timestamp();
    // explicit关键字的作用就是防止类构造函数的隐式自动转换.https://www.cnblogs.com/rednodel/p/9299251.html,explicit关键字只对有一个参数的类构造函数有效, 如果类构造函数参数大于或等于两个时, 是不会产生隐式转换的.  
    //effective c++中说：被声明为explicit的构造函数通常比其non-explicit兄弟更受欢迎。因为它们禁止编译器执行非预期（往往也不被期望）的类型转换。除非我有一个好理由允许构造函数被用于隐式类型转换，否则我会把它声明为explicit，鼓励大家遵循相同的政策。
    explicit Timestamp(int64_t microSecondsSinceEpoch);     
    static Timestamp now();                 // 输出当前时间  ,youdian xiang danli moshi,关键代码：构造函数是私有的。
    std::string toString() const;           // 格式化为字符串
private:
    int64_t microSecondsSinceEpoch_;        // 表示时间的长整形变量
};