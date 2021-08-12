#pragma once

/*
删除其拷贝构造函数和赋值函数,使其派生类不能被拷贝和赋值
*/
class noncopyable{
public:
    noncopyable(const noncopyable&) =delete;
    noncopyable& operator=(const noncopyable&) = delete;
protected:          // protected: 派生类可以访问,外部不可访问,用protect修饰的成员，跟私有成员一样，无法被外界直接访问，但是能被子类直接访问。也可以说，protect就是专门为继承而生的。
    noncopyable()=default;
    ~noncopyable() =  default;
};