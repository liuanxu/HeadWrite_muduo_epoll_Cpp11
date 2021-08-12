# 写一个shell脚本：
#1、程序必须以下面的行开始（必须方在文件的第一行）：
#!/bin/bash

# 2、注释（#注释）: #

# 3.你写的每个脚本都应该在文件开头加上set -e,这句语句告诉bash如果任何语句的执行结果不是true则应该退出。
set -e

# -e filename 如果 filename存在，则为真
# -d filename 如果 filename为目录，则为真
# -f filename 如果 filename为常规文件，则为真
# -L filename 如果 filename为符号链接，则为真
# -r filename 如果 filename可读，则为真
# -w filename 如果 filename可写，则为真
# -x filename 如果 filename可执行，则为真
# -s filename 如果文件长度不为0，则为真
# -h filename 如果文件是软链接，则为真
# 4.如果没有build目录,创建该目录
if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

# 5.首先将编译存储在build目录下的文件全部删除
rm -rf `pwd`/build/*

# &&左边的命令（命令1）返回真(即返回0，成功被执行）后，&&右边的命令（命令2）才能够被执行；
# 换句话说，“如果这个命令执行成功&&那么执行这个命令”。只有在 && 左边的命令返回真（命令返回值 $? == 0），&& 右边的命令才会被执行。
# 6.切换到build目录下进行cmake和make
cd `pwd`/build &&
    cmake .. &&
    make 

# 7.回到项目根目录
cd ..

# 把头文件拷贝到 系统的 /usr/include/mymuduo (使用时的形式为:#include<mymuduo/TcpServer>)
# 把so库拷贝到系统的 /usr/lib , 这样cmake编译时就不需要 用参数-L 包含该库
# 8. 判断系统路径是否存在该目录
if [ ! -d /usr/include/mymuduo ]; then
    mkdir /usr/include/mymuduo
fi

# 9.把当前项目目录下的头文件拷贝到/usr/include/mymuduo
for header in `ls *.h`
do
    cp $header /usr/include/mymuduo
done

# 10.把so库拷贝到 /usr/lib下
cp `pwd`/lib/libmymuduo.so /usr/lib

# 刷新动态库缓存
ldconfig

