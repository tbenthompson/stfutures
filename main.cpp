#include <cassert>
#include <iostream>
#include <queue>
#include <functional>
#include <memory>

struct Scheduler {
    std::queue<std::function<void()>> tasks;
    void run();
};

void Scheduler::run() {
    while (!tasks.empty()) {
        auto& t = tasks.front();
        t();
        tasks.pop();
    };
}

thread_local Scheduler scheduler;

template <typename T>
struct STFData {
    std::unique_ptr<T> val;
    int dependencies;
    std::vector<std::function<void(T& val)>> fulfill_triggers;
};

template <typename T>
struct STF {
    std::shared_ptr<STFData<T>> data;

    STF(): data(std::make_shared<STFData<T>>()) 
    {
        data->dependencies = 0;
    }
    
    void fulfill(T val) 
    {
        assert(data->val == nullptr);
        data->val.reset(new T(std::move(val)));
        for (auto& t: data->fulfill_triggers) {
            t(*data->val);
        }
    }
};

template <typename T>
STF<T> empty() 
{
    return STF<T>();
}

template <typename T>
STF<T> ready(T val) 
{
    auto out = empty<T>();
    out.fulfill(std::move(val));
    return out;
}

template <typename F, typename... Args>
STF<typename std::result_of<F(Args&...)>::type> 
after(F f, STF<Args>... args) 
{
    typedef typename std::result_of<F(Args&...)>::type OutType;
    auto out = empty<OutType>();
    out->data.dependencies = sizeof...(args);
    args->data.fulfill_triggers.push_back(
        [=] (Args& val) 
        {
            (void)val;
        }
    )...;
    // attach a handler to each dependency that triggers a countdown on the dependent. when that countdown is done,
    // add the task to the queue
}

template <typename F>
STF<typename std::result_of<F()>::type> async(F f)
{
    STF<typename std::result_of<F()>::type> out;
    scheduler.tasks.push([=] () mutable { out.fulfill(f()); });
    return out;
}

int main() {
    auto fut = async([] () { std::cout << "HI" << std::endl; return 11; });
    fut = after([] (int a) { std::cout << "WHAAA" << std::endl; return a + 1; }, fut);
    scheduler.run();
    return 0;
}
