#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__
#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
class Task {
public:
	virtual void run() = 0;
};

enum class ThreadMode {
	MODE_FIXED,
	MODE_CACHED,
};
class Thread {
public:
	using ThreadFunc = std::function<void()>;
	Thread(ThreadFunc func);
	~Thread();
	void start();
private:
	ThreadFunc func_;
};

class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();

	void setMode(const ThreadMode mode);
	void start(const size_t size = 4);
	void setTaskListMaxThreshold(const size_t size);
	void submitTask(std::shared_ptr<Thread> sp);
	/*
	no expect copy construct & copy assign
	*/
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool operator=(const ThreadPool&) = delete;
private:
	void threadFunc();
private:

	ThreadMode mode_;
	/*
		thread pool
	*/
	std::vector<Thread*> threadPool_;
	size_t initThreadSize_;

	/*
		task list
	*/
	std::mutex taskListMtx_;
	std::condition_variable taksListNotFull_;
	std::condition_variable taksListNotEmpty_;
	std::queue<std::shared_ptr<Task>> taskList_;
	std::atomic_uint taskSize_;
	size_t taskMaxThreshold_;



};

#endif // !__THREADPOOL_H__

