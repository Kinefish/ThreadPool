#ifndef __TASK_H__
#define __TASK_H__
#include "threadpool.h"

#include <iostream>
#include <thread>

using ulong = unsigned long long;
class MyTask : public Task {
public:
	MyTask(int begin, int end)
		:begin_(begin),
		end_(end){ }
	~MyTask() = default;
	Any run() {
		std::this_thread::sleep_for(std::chrono::seconds(2));
		ulong sum = 0;
		for (int i = begin_;i <= end_;i++) {
			sum += i;
		}
		return sum;
	}
private:
	int begin_;
	int end_;
};

#endif // !__TASK_H__
