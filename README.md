# 线程池实现

## 并发和并行

> 并发：当`cpu`时间片足够小的时候，进程/线程的切换没有感知，在同一个时间段看起来是同时发生的
>
> 并行：多核环境中，进程/线程能够在同一时刻同时运行

##  CPU\IO密集

> `CPU`密集：进程指令多以计算为主
>
> `IO`密集：进程指令多以`IO`操作为主：设备，文件（磁盘）、网络操作（网卡也是`IO`，等待客户端连接），`IO`操作可以将当前进程阻塞

- 单核环境中，`cpu`密集不适合设计成多线程，线程切换上下文消耗性能，还需要将上个线程的信息保存到下一个线程

- 多核环境中，设计成多线程程序是必要的
- `IO`密集更适合设计成多线程

## 线程池

> 在业务到来的时候再创建线程，不仅导致系统资源占用突然上升，也导致业务的响应变慢。

### 线程

> `linux`下，`pthread`创建线程，从用户空间切换到内核空间，同时分配资源
>
> `32位linux`下，每个线程栈占用`8MB`，同时用户空间占`3G`，意味着`pthread`创建的线程数量只能在`380`左右（**3*1024/8 = 384**）

- 线程的创建、销毁是一个很”重“操作
- 线程栈占用大量内存
- 线程的上下文切换要大量时间
- 大量的线程同时被唤醒的时候，会导致负载上升，宕机（相当于打游戏的时候切换进程，再切回来会导致游戏卡死，可以看作大量线程被唤醒）
- 合适的线程数量：**当前`CPU`核心数**

###  fixed模式线程池

> 线程池的线程数量固定不变，`ThreadPool`创建时根据`CPU`核心数指定

### cached模式线程池

> 线程池的线程数量根据当前的`Task`数动态变化

## 线程互斥

> `mtx.lock`
>
> 临界区代码段
>
> `mtx.unlock`
>
> `c11`中使用`oop`去自动释放锁

- `mutex`，互斥锁
  - 资源计数`0-1`

- `atomic`，原子操作

## 线程同步/通信 

> 线程执行的先后关系

- `condition_variable（c11）`，用于拥有先后关系访问临界区代码段的多线程通信
  - `cv.wait(lock);`会将当前持有的`lock`释放掉，同时当前线程进入等待状态
  - `cv.notify_all(lock);`会通知所有持有这把`lock`的线程，将这些线程从等待状态切换成阻塞状态，这些线程还需要在抢到该`lock`后，才会进入就绪状态
- `semaphore(c20)`，用于简单的表示多线程之间的先后关系，不涉及临界区代码段的访问
  - 相当于没有资源计数限制的`mutex`锁`0-N` 
  - `sem.post()`，资源计数`+1`
  - `sem.wait()`，资源计数`-1`，没有则等待
  - `semaphore sem(1)`可以实现`mutex`，区别在于`mutex`的获取和释放是在同一线程中，`sem`可以在不同线程中调用

## mutex，lock和semaphore，condition_variable

- `mutex`是互斥锁，只有`0-1`，使用时需要显示地`mtx.lock\mtx.unlock`，会有忘记释放锁的风险
- `lock`通过封装`mutex`，得到不同功能的锁，如`std::lock_guard`，出作用域自动释放锁；`std::unique_lock`，通常配合`condition_variable`使用，可以实现线程间通信
- `semaphore`是`0-N`的信号量，如果`0-1`就退化成`mutex`，拿不到资源会阻塞，所以依赖的是`mutex+cv`

# 架构

> 以动态库形式提供，`linux:*.so`，`windows:*.dll`
>
> `Thread Pool`模型：`Thread list` 充当消费者、`Task list`充当生产者。`Task list`需要线程安全

- 不同的`Task`，伴随不同的类型，需要用**多态**实现，`virtual void run() = 0;`
- 线程和任务队列之间涉及线程通信，`mutex + condition_variable`
- `fixed`下线程队列不需要考虑线程安全；`cached`下由于线程池的动态变化，需要考虑线程安全
- `_taskList`不建议使用裸指针，当用户传入临时对象的时候，出了`submitTask()`语句会导致`Task`析构，使得`_taskList`中存的是已经析构了的对象。需要将`_taskList`中的对象生命周期拉长的同时，`run()`后自动释放资源，使用`shared_ptr`

## 基本使用

- `ThreadPool pool`
- `pool.setMode(fixed|cached)`
- `pool.start()`

## 执行结果的返回

> `c17`的`Any`类型

- `Result res = pool.submitTask()`
- `res.get().Cast<类型结果>()`



# Any，Semaphore类

> 通过`c11`实现`c17`的`Any`，`Semaphore(c20)`类型
>
> `Any`用于**接收**和**返回**任意类型
>
> `Result`将`Any`，`Semaphore`封装起来

- **模板**，用于接受任意类型
- `Base`指针指向`Derive`对象，用于一个类型能够指向任何类型，需要将`Base`设计成**虚析构**
- `Any`的成员`Base`和`Derive`

- 将真正的`data`包在`Derive`中

```cpp
class Any {
public:
   	Any() = default;
    ~Any() = default;
    //base_是指针，并且是unique类型，只支持右值引用的拷贝赋值 和 拷贝构造
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(const Any&&) = default;
    Any& operator=(const Any&&) = default;
    
    //存任意类型的构造
    template<typename T>
    Any(T data)
    	：base_(std::make_unique<Derive<T>>(T data)) {} //基类指针指向派生类对象
    //用于提取data
    /*
    	data存放在Derive中，并且Any只有Base指针，所以需要支持RTTI的强转，Base*转成Derive*
    */
    template<typename T>
    T cast_() {
        Derive<T>* ptr = dynamic_cast<Derive<T>*>(base_.get());	//没必要将ptr封装成智能指针
        if(!ptr) {	//构造的是int，调用cast_<long>之类的情况
            throw "type err.";
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
            :data_(data) {}
  		T data_;
    };
    
private:
    std::unique_ptr<Base> base_;
};
```

`Semaphore`类主要通过`lock+cv`实现

```cpp
class Semaphore {
public:
    Semaphore(int limit = 0)
        :resLimit_(limit) {}
     void wait() {
         std::unique_lock<std::mutex> lock(mtx_);
         cv_.wait(lock,[this]()->bool {return resLimit_ > 0;});
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
```



# Task类

> 由用户继承，重写`run()`，通过多态执行不同的任务

```cpp
class Task {
public:
	Task()
		:res_(nullptr) { }
	void exec();
	void setResult(Result* res);
	virtual Any run() = 0;
private:
	Result* res_;	//用shared_ptr会发生交叉引用，无法释放资源，因为Result对象的生命周期 >> Task，所以用裸指针就可以了
};
```

# Result类

> 值得注意的是，提交任务到获得返回结果，在任务没有完成之前，第二条语句需要阻塞住，因此需要线程通信

```cpp
Result res = pool.sunbitTask(task);
R result = res.get().cast_<R>();
```

- `Result`类的成员方法`get()`返回一个`Any`类
- `get()`需要设计成线程通信，使用`Semaphore`类进行

用户提交任务之后，在`threadFunc`中执行任务，`task`被执行完之后，会被析构掉，如果是以`task->getResult()`形式返回`Result`对象，当调用`get()`方法时，会出现析构掉的对象调用方法的情况，因为这个方式是`Result`依赖`task`，`task`已经被析构掉了

**所以需要`Result(task)`形式返回**

```cpp
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true)
		:task_(task),
		isValid_(isValid) { 
		task_->setResult(this);
	}
	void setVal(Any any);	//从task->run中获得Any对象，赋给any_，并且post
	Any get();		//先wait，再返回any_
private:
	Semaphore sem_;
	Any any_;
	std::atomic_bool isValid_;
	std::shared_ptr<Task> task_;
};
```
