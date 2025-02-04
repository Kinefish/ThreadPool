#include <iostream>
#include "threadpool.h"

const size_t MAX_TASK_THRESHOLD = 4;
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


void ThreadPool::submitTask(std::shared_ptr<Task> sp) {
	/*
		1.get lock
		2.judge notFull
		over 1s,should call submit failed!
		3.put task
		4.nofity notEmpty
	*/
	std::unique_lock<std::mutex> lock(taskListMtx_);	//�������������ͷ�
	if (!(taskListNotFull_.wait_for(lock,
		std::chrono::seconds(1),
		[&]()->bool { return taskList_.size() < taskMaxThreshold_; })
		)) {
		std::cerr << "[time out] submit task fail." << std::endl;
		return;
	}
	taskList_.emplace(sp);
	taskSize_++;
	taskListNotEmpty_.notify_all();
}

#include <functional>
void ThreadPool::start(const size_t size) {
	initThreadSize_ = size;

	for (int i = 0; i < initThreadSize_; i++) {
		/*threadPool_.emplace_back(
			new Thread(std::bind(&ThreadPool::threadFunc, this))
		);*/
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
		threadPool_.emplace_back(	//make_unique������ֵ������emplace_back������ֵ����
			std::move(ptr)
		);
	}

	for (int i = 0; i < initThreadSize_; i++) {
		threadPool_[i]->start();
	}
}

//this thread will consume task from tasklist
void ThreadPool::threadFunc() {
	/*
		while threadPool_ not dead
		1.get lock
		2.notEmpty
		3.get task
		4.run start
	*/
	while(1) {
		std::shared_ptr<Task> task;
		{
			std::unique_lock<std::mutex> lock(taskListMtx_);

			std::cout << "tid:" << std::this_thread::get_id() <<
				"try get task" << std::endl;

			taskListNotEmpty_.wait(lock, [&]()->bool {return taskList_.size() > 0;});

			std::cout << "tid:" << std::this_thread::get_id() <<
				"get task successed..." << std::endl;

			task = taskList_.front();
			taskList_.pop();
			taskSize_--;

			if (!taskList_.empty()) taskListNotEmpty_.notify_all(); //���Լ���ִ������

			//���Լ����ύ����
			taskListNotFull_.notify_all();
		}// release lock
		
		if (task)
			task->run();
	}
}

/*
class Thread func
*/

Thread::Thread(ThreadFunc func)
	:func_(func) {
}
Thread::~Thread() {

}

#include <thread>
void Thread::start() {
	//create a thread and run threadFunc
	std::thread t(func_);
	/*
		cpp�У�thread�����������ʱ�򣬻�����û����ʾ�ص���join()/detach()
		����start()������t������Զ���������ô��Ҫ������ǰ����
		����û��Ҫ������join()���ȴ�������ɣ���˷��뼴��
	*/
	t.detach(); //set detach
}

