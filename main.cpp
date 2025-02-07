#include "threadpool.h"
#include "task.h"

static void test() {
	std::cout << "===========================" << std::endl;
	auto start = std::chrono::high_resolution_clock::now();
	ulong sum = 0;
	for (int i = 1;i <= 300000000; ++i)
		sum += i;
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	std::cout << " test " << std::endl << sum << std::endl << duration.count() << std::endl;
}

int main() {
	ThreadPool pool;
	pool.setMode(ThreadMode::MODE_CACHED);
	pool.start(2);

	Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
	Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
	pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
	pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
	pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
	pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));


	ulong sum1 = res1.get().cast_<ulong>();
	ulong sum2 = res2.get().cast_<ulong>();
	ulong sum3 = res3.get().cast_<ulong>();
	ulong ret1 = (sum1 + sum2 + sum3);

	std::cout << " threadTest " << std::endl << ret1 << std::endl;

	
	getchar();
	return 0;
}