#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__
#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>

class Any {
public:
	Any() = default;
	~Any() = default;

	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	template<typename T>
	Any(T data)
		:base_(std::make_unique<Derive<T>>(data)) { }

	template<typename T>
	T cast_() {
		Derive<T>* ptr = dynamic_cast<Derive<T>*>(base_.get());
		if (!ptr) {
			throw "type cast failed.";
		}
		return ptr->data_;
	}

private:
	class Base {
		public:
			virtual ~Base() = default;
	};

	template<typename T>
	class Derive : public Base {
	public:
		Derive(T data)
			:data_(data) {
		}
		~Derive() = default;
		T data_;
	};
private:
	std::unique_ptr<Base> base_;
};

class Semaphore {
public:
	Semaphore(int limit = 0)
		:resLimit_(limit) { }
	~Semaphore() = default;

	void wait() {
		std::unique_lock<std::mutex> lock(mtx_);
		cv_.wait(lock, [this]()->bool {return resLimit_ > 0;});
		resLimit_--;
	}

	void post() {
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cv_.notify_all();
	}

private:
	std::mutex mtx_;
	std::condition_variable cv_;
	int resLimit_;
};

class Result;
class Task {
public:
	Task()
		:res_(nullptr) { }
	void exec();
	void setResult(Result* res);
	virtual Any run() = 0;
private:
	Result* res_;	//��shared_ptr�ᷢ���������ã��޷��ͷ���Դ����ΪResult������������� >> Task����������ָ��Ϳ�����
};

class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true)
		:task_(task),
		isValid_(isValid) { 
		task_->setResult(this);
	}
	void setVal(Any any);	//��task->run�л��Any���󣬸���any_������post
	Any get();		//��wait���ٷ���any_
private:
	Semaphore sem_;
	Any any_;
	std::atomic_bool isValid_;
	std::shared_ptr<Task> task_;
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
	Result submitTask(std::shared_ptr<Task> sp);
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
	//std::vector<Thread*> threadPool_;
	std::vector<std::unique_ptr<Thread>> threadPool_;
	size_t initThreadSize_;

	/*
		task list
	*/
	std::mutex taskListMtx_;
	std::condition_variable taskListNotFull_;
	std::condition_variable taskListNotEmpty_;

	std::queue<std::shared_ptr<Task>> taskList_;
	
	std::atomic_uint taskSize_;
	size_t taskMaxThreshold_;

};

#endif // !__THREADPOOL_H__

