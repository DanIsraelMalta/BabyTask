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

#include "ThreadPool.h"
#include "TaskNode.h"
#include <list>
#include <unordered_map>

namespace BabyTask {

    /**
    * \brief task graph
    **/
    class TaskGraph {

        // properties
        ThreadPool mPool;                                   // thread pool
        std::list<std::unique_ptr<BaseTaskNode>> mNodes;    // tasks
        std::size_t mCompletedTasks{};                      // number of finished tasks
        std::mutex mMutex;
        std::condition_variable mConditionVariable;

        public:

            // constructor (notice that default is one thread in pool)
            TaskGraph(size_t xi_count = 1) noexcept : mPool(xi_count) {}

            /**
            * \brief make task nodes
            **/
            template<typename ReturnType, typename... Args>
            TaskNode<std::function<ReturnType(Args...)>, Args...>* makeTaskNode(std::function<ReturnType(Args...)> xi_task, const char* xi_name = "") {
                mNodes.emplace_back(std::make_unique<TaskNode<std::function<ReturnType(Args...)>, Args...>>(this, xi_task, xi_name));
                return static_cast<TaskNode<std::function<ReturnType(Args...)>, Args...>*>(mNodes.back().get());
            }

            template<typename... Args>
            TaskNode<std::function<void(Args...)>, Args...>* makeTaskNode(std::function<void(Args...)> xi_task, const char* xi_name = "") {
                mNodes.emplace_back(std::make_unique<TaskNode<std::function<void(Args...)>, Args...>>(this, xi_task, xi_name));
                return static_cast<TaskNode<std::function<void(Args...)>, Args...>*>(mNodes.back().get());
            }

            TaskNode<std::function<void()>>* makeTaskNode(std::function<void()> xi_task, const char* xi_name = "") {
                mNodes.emplace_back(std::make_unique<TaskNode<std::function<void()>>>(this, xi_task, xi_name));
                return static_cast<TaskNode<std::function<void()>>*>(mNodes.back().get());
            }

            /**
            * \brief test for cycles in graph
            *
            * @param {bool, out} true if a cycle was found
            **/
            bool hasCycle() const {
                std::unordered_map<BaseTaskNode*, bool> col;

                // recursive implementation of depth-first-search
                std::function<bool(BaseTaskNode*)> DFS = [&](BaseTaskNode* currentNode) -> bool {
                    col[currentNode] = true;
                    for (auto& nextNode : currentNode->mDescendants) {
                        if (!col.count(nextNode)) {
                            if (DFS(nextNode)) {
                                return true;
                            }
                        }
                        else if (col[nextNode] == true) {
                            return true;
                        }
                    }

                    col[currentNode] = false;
                    return false;
                };

                for (auto& node : mNodes) {
                    col.clear();
                    if (DFS(node.get())) {
                        return true;
                    }
                }

                return false;
            }

            /**
            * \brief reset the graph
            **/
            void reset() {
                for (auto& node : mNodes) {
                    node->reset();
                }

                mCompletedTasks = 0;
            }

            /**
            * \brief execute task graph
            **/
            void execute() {
                std::vector<BaseTaskNode*> initialNodes;

                for (auto& nodePtr : mNodes) {
                    if (!nodePtr->getPendingCount()) {
                        initialNodes.push_back(nodePtr.get());
                    }
                }

                for (auto* initialNode : initialNodes) {
                    mPool.push(std::bind(&BaseTaskNode::execute, initialNode));
                }

                {
                    std::unique_lock<std::mutex> lock(mMutex);
                    mConditionVariable.wait(lock, [this]() { return mCompletedTasks == mNodes.size(); });
                }
            }

            /**
            * \brief execute a single node
            *
            * @param {NodeType, in} node to be executed
            **/
            template<typename NodeType>
            void executeSingleNode(NodeType* xi_node) {
                mPool.push(std::bind(&NodeType::execute, xi_node));
            }

            /**
            * \brief callback to be executed when single node is completed
            **/
            void onSingleNodeCompleted() {
                std::unique_lock<std::mutex> lock(mMutex);
                ++mCompletedTasks;
                mConditionVariable.notify_all();
            }
    };
};