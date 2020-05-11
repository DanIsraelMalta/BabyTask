/**
* BabyTask - minimalistic and generic graph based task library
*
* Dan Israel Malta
**/
// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com
#include "TaskGraph.h"

// for test purposes
#include <string>
#include <assert.h>
#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>

// simple ordered task graph using one thread 
// task1 -> task3 -> task2 -> task4
void Test1() {

    // test string
    std::string out{};

    // task graph (notice that it has only one thread!)
    BabyTask::TaskGraph task_graph;

    // define tasks
    auto task1 = task_graph.makeTaskNode([&out]() -> void { out =  "task1->"; });
    auto task2 = task_graph.makeTaskNode([&out]() -> void { out += "task2->"; });
    auto task3 = task_graph.makeTaskNode([&out]() -> void { out += "task3->"; });
    auto task4 = task_graph.makeTaskNode([&out]() -> void { out += "task4";   });

    // define task order
    task2->setParent(*task1);
    task4->setParent(*task1);
    task2->setParent(*task3);
    task4->setParent(*task3);
    task3->setParent(*task1);

    // no cycles in graph
    assert(task_graph.hasCycle() == false);

    // run task graph
    task_graph.execute();

    // check task graph order
    assert(out == "task1->task3->task2->task4");
}

// simple ordered task graph using one thread
// task1 ---> task2 ---->  task3   ----> 
//              |                      |  ---> task5
//              -------->  task4   ---->
// task1 also returns another variable which is not used else where in graph
void Test2() {

    // locals
    std::int32_t num{};

    // task graph (1 thread)
    BabyTask::TaskGraph task_graph;

    // tasks
    std::function<int()> first = [&num]() -> int { num = 0;  return 13; };
    auto task1 = task_graph.makeTaskNode(first);
    // can't use: auto task1 = task_graph.makeTaskNode([&num]() -> int { num = 0;  return 13; });
    // since compiler will deduce it to be std::function<void()>

    auto task2 = task_graph.makeTaskNode([&num]() -> void { num = 1; });
    auto task3 = task_graph.makeTaskNode([&num]() -> void { num += 2; });
    auto task4 = task_graph.makeTaskNode([&num]() -> void { num *= 2; });
    auto task5 = task_graph.makeTaskNode([&num]() -> void { num %= 5; });

    // define task graph
    task2->setParent(*task1);
    task3->setParent(*task2);
    task4->setParent(*task2);
    task5->setParent(*task3);
    task5->setParent(*task4);

    // check that there are no cycles
    assert(task_graph.hasCycle() == false);

    // execute graph
    task_graph.execute();

    // check
    assert(task1->getValue() == 13);
    assert(num == 1);
}

// parallel ordered tasks:
// task 1 ---> task 3 ---               {tasks 1 and 3 run in parallel to tasks 2 and 4; but in a given order}
//                       |-> task 5
// task 2 ---> task 4 ---               {tasks 2 and 4 run in parallel to tasks 1 and 3; but in a given order}
void Test3() {

    // task graph (2 threads)
    BabyTask::TaskGraph task_graph(2);

    // locals
    std::vector<float> n0, n1;
    float n0_min{}, n1_max{}, average{35.13f};

    // task #1 - fill n0
    auto task1 = task_graph.makeTaskNode([&n0]() -> void {
        n0.resize(9'000'000);
        std::iota(n0.begin(), n0.end(), -4'500'000.0f);
    });

    // task #2 - fill n1
    auto task2 = task_graph.makeTaskNode([&n1]() -> void {
        n1.resize(9'000'000);
        std::iota(n1.begin(), n1.end(), -4'500'000.0f);
    });

    // task #3 - calculate n0 min number
    auto task3 = task_graph.makeTaskNode([&n0, &n0_min]() -> void {
        auto it = std::min_element(n0.begin(), n0.end());
        n0_min = *it;
    });

    // task #4 - calculate n1 max number
    auto task4 = task_graph.makeTaskNode([&n1, &n1_max]() -> void {
        auto it = std::max_element(n1.begin(), n1.end());
        n1_max = *it;
    });

    // task #5 - get average of task's 3 & 4 output
    auto task5 = task_graph.makeTaskNode([&n0_min, &n1_max, &average]() -> void {
        average = n0_min + 0.5f * (n1_max - n0_min);
    });

    // define task graph
    task3->setParent(*task1);
    task4->setParent(*task2);
    task5->setParent(*task4);
    task5->setParent(*task3);

    // check that there are no cycles
    assert(task_graph.hasCycle() == false);

    // execute graph
    task_graph.execute();

    // check
    assert(std::abs(static_cast<std::int32_t>(average * 10)) == 5);
}

int main() {

	Test1();
    Test2();
    Test3();

	return 1;
}