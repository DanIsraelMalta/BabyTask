/**
* BabyTask - minimalistic and generic graph based task library.
*
* The MIT License (MIT)
*
* Copyright (c) 2019 Dan Israel Malta
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
**/
#pragma once

#include "Queue.h"
#include <atomic>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace BabyTask {

    /**
    * \brief thread pool
    **/
    class ThreadPool {

        // aliases
        using taskSignature = std::function<void(std::size_t id)>;  // void task(thread running the task)

        // properties
        std::vector<std::unique_ptr<std::thread>> mThreads;     // thread 'pool'
        std::vector<std::shared_ptr<std::atomic<bool>>> mFlags; // a flag per thread, if its true then thread is finished
        Queue<taskSignature*> mQueue;                           // task queue
        std::atomic<bool> mDone;                                // thread done?
        std::atomic<bool> mStop;                                // thread stopped>
        std::atomic<std::size_t> mIdleCount;                    // amount of idle threads
        std::mutex mMutex;
        std::condition_variable mControlVariable;

        /**
        * \brief set thread #i
        *
        * @param {size_t, in} thread index
        **/
        void set_thread(std::size_t i) {
            std::shared_ptr<std::atomic<bool>> flag(mFlags[i]);

            auto f = [this, i, flag]() {
                std::atomic<bool>& flagPtr = *flag;
                taskSignature* task;
                bool isPop{ mQueue.pop(task) };

                while (true) {
                    // while queue is not empty
                    while (isPop) {
                        std::unique_ptr<taskSignature> func(task);  // delete the function (even if an exception occurred) at return
                        (*task)(i);

                        // if the thread is required to stop, return even if the queue is not empty yet
                        if (flagPtr) return;
                        isPop = mQueue.pop(task);
                    }

                    // queue is empty here, wait for the next task
                    std::unique_lock<std::mutex> lock(mMutex);
                    ++mIdleCount;
                    mControlVariable.wait(lock, [this, &task, &isPop, &flagPtr]() {
                        isPop = mQueue.pop(task);
                        return (isPop || mDone || flagPtr);
                    });

                    --mIdleCount;

                    // if queue is empty and done, return
                    if (!isPop) return;
                }
            };

            mThreads[i].reset(new std::thread(f));
        }

        // API
        public:

            // default constructor
            ThreadPool() : mIdleCount(0), mStop(false), mDone(false) {}

            // construct with a given number of threads
            ThreadPool(std::size_t xi_count) : mIdleCount(0), mStop(false), mDone(false) {
                resize(xi_count);
            }

            // destructor
            ~ThreadPool() { stop(true); }

            // copy semantics
            ThreadPool(const ThreadPool&) = delete;
            ThreadPool& operator=(const ThreadPool&) = delete;

            // move semantics
            ThreadPool(ThreadPool&&) noexcept = delete;
            ThreadPool& operator=(ThreadPool&&) noexcept = delete;

            // return number of running threads
            std::size_t size() const { return mThreads.size(); }

            // return the number of idle threads
            std::size_t idelCount() const { return mIdleCount; }

            // return reference to thread #i
            std::thread& getThread(std::size_t i) { return *this->mThreads[i]; }

            /**
            * \brief change number of threads in pool
            *
            * @param {size_t, in} new number of threads
            **/
            void resize(std::size_t xi_count) {
                if (mStop || mDone) return;

                const std::size_t prevCount{ mThreads.size() };

                if (prevCount <= xi_count) { 
                    mThreads.resize(xi_count);
                    mFlags.resize(xi_count);

                    for (std::size_t i{ prevCount }; i < xi_count; ++i) {
                        mFlags[i] = std::make_shared<std::atomic<bool>>(false);
                        set_thread(i);
                    }
                }
                else {
                    // finish 'extra/unwanted' threads
                    for (std::size_t i{ prevCount - 1 }; i >= xi_count; --i) {
                        *mFlags[i] = true;
                        mThreads[i]->detach();
                    }

                    // stop waiting threads ( so they could be deleted safely
                    {
                        std::unique_lock<std::mutex> lock(mMutex);
                        mControlVariable.notify_all();
                    }

                    mThreads.resize(xi_count);
                    mFlags.resize(xi_count);
                }
            }

            // empty task queue
            void clear_queue() {
                taskSignature* task;
                while (mQueue.pop(task)) {
                    delete task;
                }
            }

            // pop wrapper around task
            taskSignature pop() {
                taskSignature* task = nullptr;
                mQueue.pop(task);
                std::unique_ptr<taskSignature> func(task);
                taskSignature f;
                if (task) {
                    f = *task;
                }
                return f;
            }

            /**
            * \brief stop all threads (after they finished)
            *
            * @param {bool, in} if true - all tasks in queue will `run, 
            *                   otherwise - the queue will be cleared without running the tasks
             **/
            void stop(bool xi_wait = false) {
                if (!xi_wait) {
                    if (mStop) return;

                    // command the threads to stop
                    mStop = true;
                    const std::size_t len{ size() };
                    for (std::size_t i{}; i < len; ++i) {
                        *mFlags[i] = true;
                    }

                    // empty the queue
                    clear_queue();
                }
                else {
                    if (mDone || mStop) return;
                    mDone = true; 
                }

                // stop all waiting threads
                {
                    std::unique_lock<std::mutex> lock(mMutex);
                    mControlVariable.notify_all();
                }

                // wait for the computing threads to finish
                for (std::size_t i{}; i < mThreads.size(); ++i) {  
                    if (mThreads[i]->joinable()) {
                        mThreads[i]->join();
                    }
                }

                // clear task queue
                clear_queue();
                mThreads.clear();
                mFlags.clear();
            }

            /**
            * \brief push a task to queue
            *
            * @param {F,       in}  task
            * @param {Args..., in}  task arguments
            * @param {future,  out} future
            **/
            template<typename F, typename... Args>
            auto push(F&& xi_task, Args&&... xi_args) -> std::future<decltype(xi_task(0, xi_args...))> {
                auto taskPack = std::make_shared<std::packaged_task<decltype(xi_task(0, xi_args...))(std::size_t)>>(
                                    std::bind(std::forward<F>(xi_task), std::placeholders::_1, std::forward<Args>(xi_args)...)
                                );
                auto task = new taskSignature([taskPack](std::size_t id) { (*taskPack)(id); });

                mQueue.push(task);
                std::unique_lock<std::mutex> lock(mMutex);
                mControlVariable.notify_one();

                return taskPack->get_future();
            }

            template<typename F>
            auto push(F&& xi_task) -> std::future<decltype(xi_task(0))> {
                auto taskPack = std::make_shared<std::packaged_task<decltype(xi_task(0))(std::size_t)>>(std::forward<F>(xi_task));
                auto task = new taskSignature([taskPack](std::size_t id) { (*taskPack)(id); });

                mQueue.push(task);
                std::unique_lock<std::mutex> lock(mMutex);
                mControlVariable.notify_one();

                return taskPack->get_future();
            }
    };
};