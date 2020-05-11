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

#include "BaseTaskNode.h"
#include <optional>
#include <tuple>
#include <atomic>
#include <type_traits>
#include <functional>
#include <exception>

namespace BabyTask {

    /**
    * \brief a task in the graph
    *
    * @param {TaskCallback} task type
    * @param {Args...}      task arguments
    **/
    template<typename TaskCallback, typename... Args>
    class TaskNode final : public BaseTaskNode {
        // friends
        friend class TaskGraph;

        // API
        public:

            // aliases
            using ReturnType             = std::invoke_result_t<TaskCallback, Args...>;
            using ResultStorage          = typename std::conditional<!std::is_void_v<ReturnType>, ReturnType, std::size_t>::type; // size_t = placeholder type for void-returning function 
            using SubscribeCallback      = std::function<void(typename std::conditional<!std::is_void_v<ReturnType>, ReturnType, std::false_type>::type)>;
            using SubscribeNoArgCallback = std::function<void(void)>;   

            // value constructor
            explicit TaskNode(TaskGraph* xi_graph, TaskCallback xi_task, const char* xi_name) : BaseTaskNode(xi_name), 
                                                                                                mGraph(xi_graph), 
                                                                                                mTask(xi_task) {}

            // destructor
            ~TaskNode() noexcept = default;

            /**
            * \brief declare node's parent (where parent output is not current node input's)
            *
            * @param {ParentTask, in} current node parent
            **/
            template<typename ParentTask>
            void setParent(ParentTask& xi_parent) {
                updateParentCount();
                SubscribeNoArgCallback cb = std::bind(static_cast<void(TaskNode::*)(void)>(&TaskNode::onArgumentReady), this);
                xi_parent.addChild(cb);
                xi_parent.mDescendants.emplace_back(this);
            }

            /**
            * \brief execute node's task and call all the registered callbacks
            *        throw if 'ReturnType' is not copyable but there is more than one descendant
            *        that requires the result object.
            **/
            virtual void execute() override {
                // task doesn't return anything
                if constexpr (std::is_void_v<ReturnType>) {
                    if constexpr (!std::is_copy_constructible<decltype(mArguments)>::value) {
                        std::apply(mTask, std::move(mArguments));
                    } else {
                        std::apply(mTask, mArguments);
                    }
                } // task return an argument
                else {
                    if constexpr (!std::is_copy_constructible<ReturnType>::value) {
                        if (mDescendantTasks.size() > 1) {
                            throw std::logic_error("cant have more than 1 descendant process for  anon-copyable object"); 
                        }

                        mResult = std::apply(mTask, std::move(mArguments));
                        if (!mDescendantTasks.empty()) {
                            mDescendantTasks[0](std::move(mResult.value()));
                            mResult.reset();
                        }
                    }
                    else {
                        mResult = std::apply(mTask, mArguments);
                        for (size_t i{}; i < mDescendantTasks.size(); ++i) {
                            mDescendantTasks[i](mResult.value());
                        }
                    }
                }

                // call descendant's
                for (auto childTask : mDescendantArguments) {
                    childTask();
                }

                // signal graph that task is complete
                mGraph->onSingleNodeCompleted();
            }

            /**
            * \brief get current node task output
            **/
            ReturnType getValue() {
                if (!mResult) {
                    throw std::logic_error("node has no result.");
                }

                if constexpr (!std::is_copy_constructible<ReturnType>::value) {
                    return std::move(mResult.value());
                } else {
                    return mResult.value();
                }
            }

            /**
            * \brief add descendant to current node
            *
            * @param {SubscribeCallback, in} descendant callback function
            **/
            void addChild(SubscribeNoArgCallback descendant) {
                mDescendantArguments.push_back(descendant);
            }

        // internals
        private:

            // properties
            TaskCallback mTask;                                                                 // task callback
            std::tuple<Args...> mArguments;                                                     // task arguments
            std::optional<ResultStorage> mResult;                                               // task outcome
            std::atomic<size_t> mPendingCount = std::tuple_size<std::tuple<Args...>>::value;    // amount of parents which are still in queue
                                                                                                // (execute current task when this value is zero)
            std::size_t mParentCount = std::tuple_size<std::tuple<Args...>>::value;             // how many parents this node has
            std::vector<SubscribeCallback> mDescendantTasks;                                    // current node descendant's
            std::vector<SubscribeNoArgCallback> mDescendantArguments;                           // current node descendant's arguments
            TaskGraph* mGraph;                                                                  // pointer to task graph

            // BaseTaskNode interface
            virtual void reset() override { mResult.reset(); mPendingCount = mParentCount; }
            virtual std::size_t getPendingCount() const final { return mPendingCount; }

            // increment parent count
            void updateParentCount() {
                ++mParentCount;
                ++mPendingCount;
            }

            /**
            * \brief register a callback to be called when parent nodes have done their tasks
            **/
            void onArgumentReady() {
                if (--mPendingCount == 0) {
                    mGraph->executeSingleNode(this);
                }
            }
    };
};