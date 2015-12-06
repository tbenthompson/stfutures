#include <cassert>
#include <iostream>
#include <queue>
#include <stack>
#include <functional>
#include <memory>


// This curry code is based off the implementation here:
// https://stackoverflow.com/questions/152005/how-can-currying-be-done-in-c/26768388#26768388
template <typename F> struct Curry;

// specialization for functions with a single argument
template <typename Return, typename Arg> 
struct Curry<std::function<Return(Arg)>> {
    using Type = std::function<Return(Arg)>;
    const Type result;
    Curry(Type fun): result(fun) {}
};

template <typename Return, typename Arg1, typename... Args>
struct Curry<std::function<Return(Arg1,Args...)>> {
    using Remaining = typename Curry<std::function<Return(Args...)>>::Type;
    using Type = std::function<Remaining(Arg1)>;
    const Type result;
    Curry(std::function<Return(Arg1,Args...)> fun): 
        result([=](const Arg1& t) {
                return Curry<std::function<Return(Args...)>>(
                    [=](const Args&... args){ 
                        return fun(t, args...); 
                    }
                ).result;
            }
        )
    {}
};

template <typename Return, typename... Args> 
auto curry(const std::function<Return(Args...)>& f)
{
    return Curry<std::function<Return(Args...)>>(f).result;
}

template <typename Return, typename... Args> 
auto curry(Return(*f)(Args...))
{
    return Curry<std::function<Return(Args...)>>(f).result;
}

template<typename T> struct remove_class { };
template<typename C, typename R, typename... A>
struct remove_class<R(C::*)(A...)> { using type = R(A...); };
template<typename C, typename R, typename... A>
struct remove_class<R(C::*)(A...) const> { using type = R(A...); };
template<typename C, typename R, typename... A>
struct remove_class<R(C::*)(A...) volatile> { using type = R(A...); };
template<typename C, typename R, typename... A>
struct remove_class<R(C::*)(A...) const volatile> { using type = R(A...); };

template<typename T>
struct get_signature_impl { using type = typename remove_class<
    decltype(&std::remove_reference<T>::type::operator())>::type; };
template<typename R, typename... A>
struct get_signature_impl<R(A...)> { using type = R(A...); };
template<typename R, typename... A>
struct get_signature_impl<R(&)(A...)> { using type = R(A...); };
template<typename R, typename... A>
struct get_signature_impl<R(*)(A...)> { using type = R(A...); };
template<typename T> using get_signature = typename get_signature_impl<T>::type;

template<typename F> using make_function_type = std::function<get_signature<F>>;
template<typename F> make_function_type<F> make_function(F &&f) {
    return make_function_type<F>(std::forward<F>(f)); }

template <typename F>
auto curry(F f)
{
    return curry(make_function(f));
}


struct Scheduler {
    std::stack<std::function<void()>> tasks;

    template <typename F>
    void add_task(F f) 
    {
        tasks.push(f);
    }

    void run()
    {
        while (!tasks.empty()) {
            auto t = std::move(tasks.top());
            tasks.pop();
            t();
        }
    }
};

thread_local Scheduler scheduler;

template <typename T>
struct STFData {
    std::unique_ptr<T> val;
    std::vector<std::function<void(T& val)>> fulfill_triggers;
};

template <typename T>
struct STF {
    using Type = T;
    std::shared_ptr<STFData<T>> data;

    STF(): data(std::make_shared<STFData<T>>()) {}

    void fulfill(T val) 
    {
        assert(data->val == nullptr);
        data->val.reset(new T(std::move(val)));
        for (auto& t: data->fulfill_triggers) {
            t(*data->val);
        }
    }

    void add_trigger(std::function<void(T& val)> trigger)
    {
        if (data->val != nullptr) {
            trigger(*data->val);
        } else {
            data->fulfill_triggers.push_back(trigger);
        }
    }


    template <typename U>
    auto ap(STF<U> val_fut);
};

template <typename T>
auto ready(T val)
{
    STF<T> out;
    out.fulfill(std::move(val));
    return out;
}

template <typename F, typename T>
auto bind(F f, STF<T> val_fut)
{
    using OutType = std::result_of_t<decltype(f)(T)>;
    STF<OutType> future_of_future;
    val_fut.add_trigger([=] (T& val) mutable
        {
            scheduler.add_task([=] () mutable
                {
                    future_of_future.fulfill(f(val));   
                }
            );
        }
    );
    OutType out;
    future_of_future.add_trigger([=] (OutType& inner) mutable
        {
            inner.add_trigger([=] (typename OutType::Type& val) mutable
                {
                    out.fulfill(val);
                }
            );
        }
    );
    return out;
}

template <typename F, typename T>
auto fmap(F f, STF<T> val)
{
    auto f_curried = curry(f);
    return bind([=] (const T& val) 
        {
            return ready(f_curried(val));
        },
        val
    );
}

template <typename F>
template <typename T>
auto STF<F>::ap(STF<T> val_fut)
{
    return bind([=] (const F& f) 
        {
            return bind([=] (const T& val) {
                    return ready(f(val)); 
                }, 
                val_fut
            );
        },
        *this
    );
}

auto add(int x, int y)
{
    return x + y;
}

auto fib(int index)
{
    if (index < 3) {
        return ready(1);
    }
    return fmap(add, fib(index - 1)).ap(fib(index - 2));
}

int main() {
    auto print = [] (int v) { std::cout << v << std::endl; return 0; };
    fmap(print, fib(25));

    // auto mult = [] (int x, int y) { return x * y; };
    // auto fut = fmap([] (int x) { std::cout << "HI" << std::endl; return x; }, ready(4));
    // fmap(print, fut);
    // // print <$> (mult <$> fut <*> STF 10)
    // fmap(print, ap(fmap(mult, fut), ready<int>(10)));
    // auto react = [] (int x) { 
    //     if (x < 5) {
    //         return ready(x); 
    //     } else {
    //         return ready(20);
    //     } 
    // };
    // do
    //     x <- fut
    //     y <- if (x < 5) x else 20
    //     z <- print y
    //     return z
    // auto bound = bind(react, fut);
    // fmap(print, bound);

    scheduler.run();
    return 0;
}
