#ifndef __TASK_H__
#define __TASK_H__
#include "threadpool.h"

#include <iostream>
#include <thread>

class MyTask : public Task {
public:
	void run() {
		std::cout << "tid:" << std::this_thread::get_id() << "begin!" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		std::cout << "tid:" << std::this_thread::get_id() << "end!" << std::endl;
	}
};

#endif // !__TASK_H__
