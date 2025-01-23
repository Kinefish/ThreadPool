#include "threadpool.h"

#include <chrono>
#include <thread>
int main() {
	ThreadPool pool;
	pool.start();

	std::this_thread::sleep_for(std::chrono::seconds(3));
	return 0;
}