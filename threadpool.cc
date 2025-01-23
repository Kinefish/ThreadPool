#include "threadpool.h"

const size_t MAX_TASK_THRESHOLD = 1024;
ThreadPool::ThreadPool()
	:initThreadSize_(0),
	taskSize_(0),
	taskMaxThreshold_(MAX_TASK_THRESHOLD),
	mode_(ThreadMode::MODE_FIXED) { }

ThreadPool::~ThreadPool() {}

void ThreadPool::setMode(const ThreadMode mode) {
	mode_ = mode;
}

void ThreadPool::setTaskListMaxThreshold(const size_t size) {
	taskMaxThreshold_ = size;
}

void ThreadPool::submitTask(std::shared_ptr<Thread> sp) {

}

#include <functional>
void ThreadPool::start(const size_t size) {
	initThreadSize_ = size;

	for (int i = 0; i < initThreadSize_; i++) {
		threadPool_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));
	}

	for (int i = 0; i < initThreadSize_; i++) {
		threadPool_[i]->start();
	}
}

#include <iostream>
void ThreadPool::threadFunc() {
	std::cout << "tid:" << std::this_thread::get_id() << "begin" << std::endl;
	std::cout << "tid:" << std::this_thread::get_id() << "end" << std::endl;
}

/*
class Thread func
*/

Thread::Thread(ThreadFunc func)
	:func_(func) { }

#include <thread>
void Thread::start() {
	//create a thread and run threadFunc
	std::thread t(func_);
	t.detach(); //set detach
}