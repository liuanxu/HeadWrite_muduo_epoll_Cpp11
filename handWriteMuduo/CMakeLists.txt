cmake_minimum_required(VERSION 2.5)

project(mymuduo)



# mymuduo 最终编译成so动态库,设置动态库输出路径,放在根目录的lib文件夹下面
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置调试信息
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# 定义参与编译的源代码文件,.:当前目录所有源文件定义为变量SRC_LIST
aux_source_directory(. SRC_LIST)

#编译生成动态库mymuduo
add_library(mymuduo SHARED ${SRC_LIST})
