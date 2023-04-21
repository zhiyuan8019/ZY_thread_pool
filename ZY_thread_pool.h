//
// Created by zhenyang on 2023/4/15.
//

#ifndef ZY_THREAD_POOL_ZY_THREAD_POOL_H
#define ZY_THREAD_POOL_ZY_THREAD_POOL_H
#endif //ZY_THREAD_POOL_ZY_THREAD_POOL_H

#include <thread>
#include <type_traits>
#include <atomic>
#include <functional>
#include <vector>
#include <future>
#include <queue>


namespace ZY {
    using thread_n = std::result_of<decltype(&std::thread::hardware_concurrency)()>::type;


    template<typename Tp>
    using decay_t = typename std::decay<Tp>::type;
    template<typename Tp>
    using result_of_t = typename std::result_of<Tp>::type;
    template<typename F, typename... Args>
    using result_t = result_of_t<decay_t<F>(decay_t<Args>...)>;

    class thread_pool {
    public:
        explicit thread_pool(const thread_n thread_num_ = 0) {
            init(thread_num_);
        }

        thread_pool(const thread_pool &) = delete;

        thread_pool(thread_pool &&) = delete;

        thread_pool &operator=(const thread_pool &) = delete;

        thread_pool &operator=(thread_pool &&) = delete;

        ~thread_pool() {
            close();
        }


        template<typename F, typename... Args>
        void push(F &&f, Args &&... args) {
            std::function<void()> task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
            {
                std::lock_guard<std::mutex> task_lock(queue_mutex);
                tasks_queue.push(std::move(task));
            }
            ++task_total_num;
            task_available_cv.notify_one();
        }

        template<typename R>
        typename std::enable_if<std::is_void<R>::value>::type
        set_result(std::shared_ptr<std::promise<R>> promise, std::function<R()> task_function) {
            try {
                task_function();
                promise->set_value();
            } catch (...) {
                try {
                    promise->set_exception(std::current_exception());
                } catch (...) {

                }
            }
        }

        template<typename R>
        typename std::enable_if<!std::is_void<R>::value>::type
        set_result(std::shared_ptr<std::promise<R>> promise, std::function<R()> task_function) {
            try {
                promise->set_value(task_function());
            } catch (...) {
                try {
                    promise->set_exception(std::current_exception());
                } catch (...) {

                }
            }
        }


        template<typename F, typename... Args, typename R = result_t<F, Args...> >
        std::future<R> submit(F &&f, Args &&... args) {
            std::function<R()> task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
            std::shared_ptr<std::promise<R>> task_promise = std::make_shared<std::promise<R>>();
            push([task_promise, task, this]() {
                set_result(task_promise, task);
//                try {
//                    if (std::is_void<R>::value) {
//                        task();
//                        task_promise->set_value(void());
//                    } else {
//                        task_promise->set_value(task());
//                    }
//
//                } catch (...) {
//                    try {
//                        task_promise->set_exception(std::current_exception());
//                    } catch (...) {
//
//                    }
//                }
            });
            return task_promise->get_future();
        }

        void join() {
            joining = true;
            std::unique_lock<std::mutex> join_lock(queue_mutex);
            task_finish_cv.wait(join_lock, [this]() { return task_total_num == task_finish_num; });
            joining = false;
        }

        void reset(thread_n thread_num_) {
            close();
            running = true;
            init(thread_num_);
        }

        void close() {
            join();
            running = false;
            task_available_cv.notify_all();
            for (auto &thread: threads) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
        }


    private:
        /**
         * @brief
         */
        void worker() {
            while (running) {
                std::function<void()> task;
                std::unique_lock<std::mutex> thread_lock(queue_mutex);
                task_available_cv.wait(thread_lock, [this]() {
                    return !tasks_queue.empty() || !running;
                });
                //lock1
                if (running) {
                    task = std::move(tasks_queue.front());
                    tasks_queue.pop();
                    thread_lock.unlock();//unlock1
                    task();
                    ++task_finish_num;
                    if (joining)task_finish_cv.notify_one();
                }
                //auto unlock
            }


        }

        void init(thread_n thread_num_) {
            cal_thread_num(thread_num_);
            threads.reserve(thread_num);
            for (thread_n i = 0; i < thread_num; ++i) {
                threads.emplace_back(&thread_pool::worker, this);
            }
        }

        void cal_thread_num(thread_n give_num) {
            if (give_num > 0) {
                thread_num = give_num;
            } else {
                if (std::thread::hardware_concurrency() > 0) {
                    thread_num = std::thread::hardware_concurrency();
                } else {
                    thread_num = 1;
                }
            }
        }

        std::atomic<thread_n> thread_num = {0};
        std::atomic<size_t> task_total_num = {0};
        std::atomic<size_t> task_finish_num = {0};
        std::atomic<bool> running = {true};
        std::atomic<bool> joining = {false};

        std::mutex queue_mutex;
        std::condition_variable task_available_cv;
        std::condition_variable task_finish_cv;

        std::vector<std::thread> threads;
        std::queue<std::function<void()>> tasks_queue;


    };

}// namespace ZY
