#include <cassert>
#include <iostream>
#include <queue>
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

// CURRYING!
//https://stackoverflow.com/questions/152005/how-can-currying-be-done-in-c/26768388#26768388
//https://gist.github.com/ivan-cukic/6269914
//https://www.reddit.com/r/cpp/comments/1kkkne/currying_in_c11/
//https://github.com/LeszekSwirski/cpp-curry

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
    std::vector<std::function<void(T& val)>> fulfill_triggers;
};

template <typename T>
struct STF;

template <typename F>
static auto async(F f)
{
    auto out = STF<typename std::result_of_t<F()>>::empty();
    scheduler.tasks.push([=] () mutable { out.fulfill(f()); });
    return out;
}

template <typename T>
struct STF {
    using Type = T;
    std::shared_ptr<STFData<T>> data;

    STF(): data(std::make_shared<STFData<T>>()) {}

    static auto empty() 
    {
        return STF<T>();
    }

    static auto pure(T val) 
    {
        auto out = empty();
        out.fulfill(std::move(val));
        return out;
    }
    
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

    template <typename F>
    auto then_impl(F f, auto future) 
    {
        add_trigger([=] (T& val) mutable
            {
                scheduler.tasks.push([=] () mutable
                    {
                        future.fulfill(f(val));   
                    }
                );
            }
        );
    }
};

template <typename F, typename T>
auto fmap(F f, STF<T> val)
{
    auto f_curried = curry(f);
    auto out = STF<std::result_of_t<decltype(f_curried)(T)>>::empty();
    val.then_impl(f_curried, out);
    return out;
}

template <typename F, typename T>
auto ap(STF<F> f_fut, STF<T> val_fut)
{
    auto out = STF<std::result_of_t<F(T)>>::empty();
    f_fut.add_trigger([=] (F& f) mutable
        {
            val_fut.then_impl(f, out);
        }
    );
    return out;
}

template <typename T>
auto unwrap(STF<STF<T>> in)
{
    auto out = STF<T>::empty();
    in.add_trigger([=] (STF<T>& inner) mutable
        {
            inner.add_trigger([=] (T& val) mutable
                {
                    out.fulfill(val);
                }
            );
        }
    );
    return out;
}

template <typename F, typename T>
auto bind(F f, STF<T> val_fut)
{
    auto out = STF<std::result_of_t<F(T)>>::empty();
    val_fut.then_impl(f, out);
    return unwrap(out);
}


int main() {
    auto print = [] (int v) { std::cout << v << std::endl; return 0; };
    auto mult = [] (int x, int y) { return x * y; };
    auto fut = async([] () { std::cout << "HI" << std::endl; return 11; });
    fmap(print, fut);
    // print <$> (mult <$> fut <*> STF 10)
    fmap(print, ap(fmap(mult, fut), STF<int>::pure(10)));
    auto react = [] (int x) { if (x < 5) { return STF<int>::pure(x); } else { return async([]() {return 20;}); } };
    auto bound = bind(react, fut);
    fmap(print, bound);

    scheduler.run();
    return 0;
}
