#include "utils.h"
#include <array>
#include <functional>
#include <thread>
#include <bitset>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <future>
#include "minilog.hh"

using namespace std;
using enum LogLevel;
template <size_t N>
class ThreadPool
{
    condition_variable cv_;
    mutex mtx_;
    queue<function<void()>> tasks_;
    vector<thread> workers;

public:
    ThreadPool() : cv_(), mtx_(), tasks_()
    {
        workers.reserve(N);
        for (int i{0}; i < N; ++i)
        {
            workers.emplace_back([&]
                                 {
            while(true){
                unique_lock<mutex>lk(mtx_);
                while(tasks_.empty()){
                    cv_.wait(lk);
                }
                auto task{std::move(tasks_.front())};
                tasks_.pop();
                lk.unlock();
                cv_.notify_one();
                cout<<"";
                printf("***************thread %lld: task begin******************\n",this_thread::get_id());
                task();
                printf("***************thread %lld: task end******************\n",this_thread::get_id());

            } });
        }
    }
    template <typename Func, typename... Args>
    auto submit(Func &&func, Args &&...args) -> future<decltype(func(args...))>
    {
        using rettype = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<rettype()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        future<rettype> result{task->get_future()};

        unique_lock<mutex> lk(mtx_);
        tasks_.emplace([task]()
                       { (*task)(); });
        //lg.log(Debug,"task size:{} ",tasks_.size());
        cv_.notify_one();
        return result;
    }
};