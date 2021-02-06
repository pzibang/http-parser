cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

# 设置工程名字
project(PROJECT)

#设置变量TARGETS，其内容为"test"
set(TARGETS "test")

set(SUBDIRS "")
foreach(dir ${SUBDIRS})
	aux_source_directory(${PROJECT_SOURCE_DIR}/${dir} ALL_SRC)
	include_directories(${PROJECT_SOURCE_DIR}/${dir}/include)
endforeach()
include_directories(${PROJECT_SOURCE_DIR})
aux_source_directory(. MAIN_SRC)


foreach(tmp ${ALL_SRC})
	message(STATUS "${tmp}")
endforeach()


#设置输出目录
set(OUTDIRS "build") # Debug:调试 Release:不调试
set(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/${OUTDIRS}/lib/")
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/${OUTDIRS}/bin/")


# 添加对gdb的支持
SET(CMAKE_BUILD_TYPE "Debug")
#判断编译器类型,如果是gcc编译器,则在编译选项中加入c++11支持
if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "-std=c++11 -lpthread -Wall")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g -ggdb")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")
endif(CMAKE_COMPILER_IS_GNUCXX)


#生成可执行文件
#add_executable(${TARGETS} ${ADD_SRC} ${SUB_SRC} ${MAIN_SRC})
add_executable(${TARGETS} ${ALL_SRC} ${MAIN_SRC})


#提示错误：undefined reference to `pthread_create' collect2: error: ld returned 1 exit status
#解决：需要添加 pthread 库
find_package(Threads)
target_link_libraries(${TARGETS} ${CMAKE_THREAD_LIBS_INIT})
