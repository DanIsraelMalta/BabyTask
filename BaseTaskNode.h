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

#include <vector>
#include <string>

namespace BabyTask {

    /**
    * \brief task graph node interface
    **/
    class BaseTaskNode {
        public:

            BaseTaskNode(const char* name) noexcept : mName(name) {}
            virtual ~BaseTaskNode() noexcept = default;

            /**
            * \brief execute node task
            **/
            virtual void execute() = 0;

            /**
            * \brief return number of pending tasks
            **/
            virtual std::size_t getPendingCount() const = 0;

            /**
            * \brief return task name
            *
            * @param {string, out} node name
            **/
            const std::string& getName() const { return mName; }

            /**
            * \brief reset node
            **/
            virtual void reset() = 0;

            // descendant nodes (will be executed once this node has finished)
            std::vector<BaseTaskNode*> mDescendants;

        // internals
        protected:
            std::string mName;
    };
};