#include "threadpool_recstr.h"

using ulong = unsigned long long;
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

static int sum(int a, int b) {
	std::this_thread::sleep_for(std::chrono::seconds(2));
	return a + b;
}
static int sum2(int a, int b, int c) {
	std::this_thread::sleep_for(std::chrono::seconds(2));
	return a + b + c;
}
/*
packaged_task是一个函数对象
优化成
pool.submitTask(sum,10,29,39);
pool.submitTask(sum,10,29);
改成可变参模板
*/
static void futureTest() {
	std::packaged_task<int(int,int)> task_1(sum);
	task_1(18, 28);

	std::future<int> sum_1 = task_1.get_future();

	std::cout << sum_1.get() << std::endl;
	return;
}

static void recstrTest() {
	ThreadPool_RECSTR pool;
	pool.setMode(ThreadMode_RECSTR::MODE_CACHED);
	pool.start(2);

	std::future<int> res1 = pool.submitTask(sum, 1, 2);
	std::future<int> res2 = pool.submitTask(sum2,1, 2, 3);
	std::future<int> res3 = pool.submitTask(sum2,1, 2, 3);
	std::future<int> res4 = pool.submitTask(sum2,1, 2, 3);
	std::future<int> res5 = pool.submitTask(sum2,1, 2, 3);


	std::cout << "====" << res1.get() << std::endl;
	std::cout << "====" << res2.get() << std::endl;
	std::cout << "====" << res3.get() << std::endl;
	std::cout << "====" << res4.get() << std::endl;
	std::cout << "====" << res5.get() << std::endl;

}
int main() {

	recstrTest();
	getchar();
	return 0;
}