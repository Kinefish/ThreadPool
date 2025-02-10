#ifndef __THREADPOOL_RECSTR_H__
#define __THREADPOOL_RECSTR_H__

#include <vector>
#include <queue>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>

const size_t MAX_TASK_THRESHOLD = 2;
const size_t MAX_THREAD_THRESHOLD = INT32_MAX;
const size_t IDLETHREAD_TIME_OUT = 60;

enum class ThreadMode_RECSTR {
	MODE_FIXED,
	MODE_CACHED,
};
class Thread_RECSTR {
public:
	using ThreadFunc = std::function<void(int)>;
	Thread_RECSTR(ThreadFunc func) 
		:func_(func),
		threadId_(generateThreadId_++) {
	}
	~Thread_RECSTR() = default;
	void start() {
		// create a thread and run threadFunc
			std::thread t(func_, threadId_);
		/*
			cpp�У�thread�����������ʱ�򣬻�����û����ʾ�ص���join()/detach()
			����start()������t������Զ���������ô��Ҫ������ǰ����
			����û��Ҫ������join()���ȴ�������ɣ���˷��뼴��
		*/
		t.detach(); //set detach
	}
	int getThreadId() const {
		return threadId_;
	}
private:
	ThreadFunc func_;
	static int generateThreadId_;
	int threadId_;
};
int Thread_RECSTR::generateThreadId_ = 0;

class ThreadPool_RECSTR {
public:
	ThreadPool_RECSTR()
		:initThreadSize_(0),
		taskSize_(0),
		idleThreadSize_(0),
		curThreadSize_(0),
		isPoolRunning_(false),
		ThreadSizeThreshold_(MAX_THREAD_THRESHOLD),
		taskMaxThreshold_(MAX_TASK_THRESHOLD),
		mode_(ThreadMode_RECSTR::MODE_FIXED) {
	}
	~ThreadPool_RECSTR() {
		isPoolRunning_ = false;
		std::unique_lock<std::mutex> lock(taskListMtx_);	//��ȡ��
		taskListNotEmpty_.notify_all();	//�ٻ��ѣ���������
		exitState_.wait(lock, [this]()->bool {return curThreadSize_ == 0;});
	}

	void setMode(const ThreadMode_RECSTR mode) {
		if (setRunningState())
			return;
		mode_ = mode;
	}
	void start(const size_t size = std::thread::hardware_concurrency()) {
		isPoolRunning_ = true;
		initThreadSize_ = size;
		for (int i = 0; i < initThreadSize_; i++) {
			/*threadPool_.emplace_back(
				new Thread(std::bind(&ThreadPool::threadFunc, this))
			);*/
			auto ptr = std::make_unique<Thread_RECSTR>(std::bind(&ThreadPool_RECSTR::threadFunc, this, std::placeholders::_1));
			threadPool_.emplace(	//make_unique������ֵ������emplace_back������ֵ����
				ptr->getThreadId(), std::move(ptr)
			);
		}

		for (int i = 0; i < initThreadSize_; i++) {
			threadPool_[i]->start();
			idleThreadSize_++;
			curThreadSize_++;
		}
	}
	void setTaskListMaxThreshold(const size_t size) {
		if (setRunningState())
			return;
		taskMaxThreshold_ = size;
	}
	void setThreadThreshold(const size_t size) {
		if (setRunningState())
			return;
		if (mode_ == ThreadMode_RECSTR::MODE_CACHED)
			ThreadSizeThreshold_ = size;
	}

	/*
		submitTask(func, args...);
		return future<rType>����
		packaged_task<int(int, int)> task(sum)
	*/
	template<typename Func, typename... Args>
	auto submitTask(Func/*&&*/ func, Args/*&&*/... args) -> std::future<decltype(func(args...))> {
		//�������
		using rType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<rType()>>(
			std::bind(/*std::forward<Func>(func)*/func, /*std::forward<Args>(args)...*/args...)
		);
		std::future<rType> res = task->get_future();
		//ȡ��
		std::unique_lock<std::mutex> lock(taskListMtx_);
		//��ʱ�ύ
		if (!(taskListNotFull_.wait_for(lock, std::chrono::seconds(1),
			[this]()->bool {return taskSize_ < taskMaxThreshold_;}))) {
			std::cerr << "[time out] submit task fail." << std::endl;
			auto task = std::make_shared<std::packaged_task<rType()>>(
				[]()->rType { return rType(); }
			);
			(*task)();	//֧����ʽ������
			return task->get_future();
		}
		//�ύ
		taskList_.emplace([task]()->void { (*task)(); });
		taskSize_++;
		taskListNotEmpty_.notify_all();
		//CACHED�������߳�
		if (mode_ == ThreadMode_RECSTR::MODE_CACHED &&
			taskSize_ > idleThreadSize_ &&
			curThreadSize_ < ThreadSizeThreshold_) {
			auto ptr = std::make_unique<Thread_RECSTR>(std::bind(&ThreadPool_RECSTR::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getThreadId();
			threadPool_.emplace(
				threadId, std::move(ptr)
			);
			threadPool_[threadId]->start();
			curThreadSize_++;
			idleThreadSize_++;
			std::cout << "[new] thread create" << std::endl;
		}

		//����future�����û��̵߳���get()֧�������Ƚ��
		return res;
	}

	/*
	no expect copy construct & copy assign
	*/
	ThreadPool_RECSTR(const ThreadPool_RECSTR&) = delete;
	ThreadPool_RECSTR operator=(const ThreadPool_RECSTR&) = delete;
private:
	void threadFunc(int threadId) {
		auto timeStart = std::chrono::high_resolution_clock::now();
		while (1) {
			TASK task;
			{
				std::unique_lock<std::mutex> lock(taskListMtx_);
				std::cout << "tid:" << std::this_thread::get_id() <<
					"try get task" << std::endl;

				//��taskSize_ == 0��ʱ��ѭ������
				while (taskSize_ == 0) {
					if (!isPoolRunning_) {	//��֤���������ɲŻ���
						threadPool_.erase(threadId); curThreadSize_--;
						exitState_.notify_all();
						std::cout << "[exit] threadId:" << std::this_thread::get_id() << std::endl;
						return;
					}

					if (mode_ == ThreadMode_RECSTR::MODE_FIXED) {
						taskListNotEmpty_.wait(lock);
					}
					else {	//MODE_CACHED
						if (std::cv_status::timeout ==
							taskListNotEmpty_.wait_for(lock, std::chrono::seconds(1))) {
							auto now = std::chrono::high_resolution_clock::now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - timeStart);
							if (dur.count() >= IDLETHREAD_TIME_OUT &&
								curThreadSize_ > initThreadSize_) {
								threadPool_.erase(threadId);
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

				if (taskSize_ > 0) taskListNotEmpty_.notify_all(); //���Լ���ִ������

				//���Լ����ύ����
				taskListNotFull_.notify_all();
			}// release lock

			if (task)
				task();
			timeStart = std::chrono::high_resolution_clock::now();
			idleThreadSize_++;
		}//end while
	}
	bool setRunningState() {
		return isPoolRunning_;
	}
private:
	std::condition_variable exitState_;
	std::atomic_bool isPoolRunning_;
	ThreadMode_RECSTR mode_;
	/*
		thread pool
	*/
	//std::vector<Thread*> threadPool_;
	//std::vector<std::unique_ptr<Thread>> threadPool_;	//��Ȼstl��size()�����ṩ��ǰ�߳����������ǲ����̰߳�ȫ��
	std::unordered_map<int, std::unique_ptr<Thread_RECSTR>> threadPool_;	//cachedģʽ�µ�idӳ��
	size_t initThreadSize_;
	std::atomic_int idleThreadSize_;
	std::atomic_int ThreadSizeThreshold_;
	std::atomic_int curThreadSize_;

	/*
		task list
	*/
	std::mutex taskListMtx_;
	std::condition_variable taskListNotFull_;
	std::condition_variable taskListNotEmpty_;

	using TASK = std::function<void()>;
	std::queue<TASK> taskList_;

	std::atomic_uint taskSize_;
	size_t taskMaxThreshold_;

};

#endif // !__THREADPOOL_RECSTR_H__
