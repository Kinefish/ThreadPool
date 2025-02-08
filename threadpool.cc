#include <iostream>
#include "threadpool.h"

const size_t MAX_TASK_THRESHOLD = 1024;
const size_t MAX_THREAD_THRESHOLD = INT32_MAX;
const size_t IDLETHREAD_TIME_OUT = 60;

ThreadPool::ThreadPool()
	:initThreadSize_(0),
	taskSize_(0),
	idleThreadSize_(0),
	curThreadSize_(0),
	isPoolRunning_(false),
	ThreadSizeThreshold_(MAX_THREAD_THRESHOLD),
	taskMaxThreshold_(MAX_TASK_THRESHOLD),
	mode_(ThreadMode::MODE_FIXED) { }

ThreadPool::~ThreadPool() {
	isPoolRunning_ = false;
	std::unique_lock<std::mutex> lock(taskListMtx_);	//先取锁
	taskListNotEmpty_.notify_all();	//再唤醒，避免死锁
	exitState_.wait(lock, [this]()->bool {return curThreadSize_ == 0;});
}

void ThreadPool::setMode(const ThreadMode mode) {
	if (setRunningState())
		return;
	mode_ = mode;
}

void ThreadPool::setTaskListMaxThreshold(const size_t size) {
	if (setRunningState())
		return;
	taskMaxThreshold_ = size;
}

void ThreadPool::setThreadThreshold(const size_t size) {
	if (setRunningState())
		return;
	if(mode_ == ThreadMode::MODE_CACHED)
		ThreadSizeThreshold_ = size;
}

bool ThreadPool::setRunningState() {
	return isPoolRunning_;
}


Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {
	/*
		1.get lock
		2.judge notFull
		over 1s,should call submit failed!
		3.put task
		4.nofity notEmpty
	*/
	std::unique_lock<std::mutex> lock(taskListMtx_);	//出作用域会解锁释放
	 
	if (!(taskListNotFull_.wait_for(lock,
		std::chrono::seconds(1),
		[this]()->bool { return taskSize_ < taskMaxThreshold_; })
		)) {
		std::cerr << "[time out] submit task fail." << std::endl;
		return Result(sp, false);
	}
	taskList_.emplace(sp);
	taskSize_++;
	taskListNotEmpty_.notify_all();

	if (mode_ == ThreadMode::MODE_CACHED &&
		taskSize_ > idleThreadSize_ &&
		curThreadSize_ < ThreadSizeThreshold_) {
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getThreadId();
		threadPool_.emplace(
			threadId,	std::move(ptr)
		);
		threadPool_[threadId]->start();
		curThreadSize_++;
		idleThreadSize_++;
		std::cout << "[new] thread create" << std::endl;
	}

	return Result(sp);
}

#include <functional>
void ThreadPool::start(const size_t size) {
	isPoolRunning_ = true;
	initThreadSize_ = size;
	for (int i = 0; i < initThreadSize_; i++) {
		/*threadPool_.emplace_back(
			new Thread(std::bind(&ThreadPool::threadFunc, this))
		);*/
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		threadPool_.emplace(	//make_unique返回右值，并且emplace_back接受右值引用
			ptr->getThreadId(), std::move(ptr)
		);
	}

	for (int i = 0; i < initThreadSize_; i++) {
		threadPool_[i]->start();
		idleThreadSize_++;
		curThreadSize_++;
	}
}

//this thread will consume task from tasklist
void ThreadPool::threadFunc(int thraedId) {
	/*
		while threadPool_ not dead
		cached mode judge overtime
		1.get lock
		2.notEmpty
		3.get task
		4.run start
	*/
	auto timeStart = std::chrono::high_resolution_clock::now();
	while(1) {
		std::shared_ptr<Task> task;
		{
			std::unique_lock<std::mutex> lock(taskListMtx_);
			std::cout << "tid:" << std::this_thread::get_id() <<
				"try get task" << std::endl;
			
			//在taskSize_ == 0的时候循环阻塞
			while (taskSize_ == 0) {
				if (!isPoolRunning_) {	//保证任务必须完成才回收
					threadPool_.erase(thraedId); curThreadSize_--;
					exitState_.notify_all();
					std::cout << "[exit] threadId:" << std::this_thread::get_id() << std::endl;
					return;
				}

				if (mode_ == ThreadMode::MODE_FIXED) {
					taskListNotEmpty_.wait(lock);
				} else {	//MODE_CACHED
					if (std::cv_status::timeout ==
						taskListNotEmpty_.wait_for(lock, std::chrono::seconds(1))) {
						auto now = std::chrono::high_resolution_clock::now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - timeStart);
						if (dur.count() >= IDLETHREAD_TIME_OUT &&
							curThreadSize_ > initThreadSize_) {
							threadPool_.erase(thraedId);
							curThreadSize_--;
							idleThreadSize_--;
							std::cout << "[ot exit] threadId:" << std::this_thread::get_id() << std::endl;
							return;	//end this thread
						}
					}
				}
			}

			idleThreadSize_--;
			std::cout << "tid:" << std::this_thread::get_id() <<
				"get task successed..." << std::endl;

			task = taskList_.front();
			taskList_.pop();
			taskSize_--;

			if (taskSize_ > 0) taskListNotEmpty_.notify_all(); //可以继续执行任务

			//可以继续提交任务
			taskListNotFull_.notify_all();
		}// release lock
		
		if (task)
			task->exec();
		timeStart = std::chrono::high_resolution_clock::now();
		idleThreadSize_++;
	}//end while
}

/*
class Thread func
*/

Thread::Thread(ThreadFunc func)
	:func_(func),
	threadId_(generateThreadId_++){
}
Thread::~Thread() {

}
int Thread::generateThreadId_ = 0;

#include <thread>
void Thread::start() {
	//create a thread and run threadFunc
	std::thread t(func_, threadId_);
	/*
		cpp中，thread类对象析构的时候，会检查有没有显示地调用join()/detach()
		出了start()作用域，t对象会自动析构，那么就要在析构前调用
		但是没必要到调用join()来等待任务完成，因此分离即可
	*/
	t.detach(); //set detach
}

/*
class Task func
*/
void Task::exec() {
	res_->setVal(run());
}
void Task::setResult(Result* res) {
	res_ = res;
}

/*
class Result func
*/
void Result::setVal(Any any) {
	any_ = std::move(any);
	sem_.post();
}

Any Result::get() {
	if (!isValid_)
		return "";
	sem_.wait();
	return std::move(any_);
}
