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

#include <type_traits>
#include <mutex>
#include <queue>

namespace BabyTask {

    /**
    * \brief thread safe queue
    *
    * @param {T} queue held element underlying type
    **/
    template<typename T> class Queue {

        // internal
        std::queue<T> mQueue;
        std::mutex mMutex;

        // API
        public:

            // aliases
            using value_type      = typename std::queue<T>::value_type;
            using size_type	      = typename std::queue<T>::size_type;
            using reference	      = typename std::queue<T>::reference;
            using const_reference = typename std::queue<T>::const_reference;

            // default constructor
            constexpr explicit Queue() = default;

            // copy semantics
            Queue(const Queue&) = delete;
            Queue& operator=(const Queue&) = delete;

            // move semantics
            Queue(Queue&&) noexcept = delete;
            Queue& operator=(Queue&&) noexcept = delete;

            /**
            * \brief push new element to queue
            *
            * @param {T,    in}  element to pushed to queue
            * @param {bool, out} true if operation was successful
            **/
            template<typename Q = T, typename std::enable_if<std::is_copy_constructible<Q>::value>::type* = nullptr>
            constexpr bool push(const Q& xi_element) {
                std::unique_lock<std::mutex> lock(mMutex);
                mQueue.push(xi_element);
                return true;
            }

            template<typename Q = T, typename std::enable_if<std::is_move_constructible<Q>::value>::type* = nullptr>
            constexpr bool push(Q&& xi_element) {
                std::unique_lock<std::mutex> lock(mMutex);
                mQueue.emplace(std::forward<Q>(xi_element));
                return true;
            }

            /**
            * \brief pop (remove and return) queue front element
            *
            * @param {T,    out} queue front element
            * @param {bool, out} true if operation was successful
            **/
            template<typename Q = T, typename std::enable_if<std::is_copy_assignable<Q>::value && 
                                                            !std::is_move_assignable<Q>::value>::type* = nullptr>
            constexpr bool pop(Q& xo_element) {
                std::unique_lock<std::mutex> lock(mMutex);
                if (mQueue.empty()) {
                    return false;
                }

                xo_element = mQueue.front();
                mQueue.pop();

                return true;
            }

            template<typename Q = T, typename std::enable_if<std::is_move_assignable<Q>::value>::type* = nullptr>
            constexpr bool pop(Q& xo_element) {
                std::unique_lock<std::mutex> lock(mMutex);
                if (mQueue.empty()) {
                    return false;
                }

                xo_element = std::move(mQueue.front());
                mQueue.pop();

                return true;
            }

            /**
            * \brief test if queue is empty
            *
            * @param {bool, out} true if queue is empty
            **/
            constexpr bool empty() const {
                std::unique_lock<std::mutex> lock(mMutex);
                return mQueue.empty();
            }
    };
};