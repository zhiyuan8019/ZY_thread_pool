#include <iostream>
#include <cstdio>
#include "ZY_thread_pool.h"


auto task_print_id = [](int id_) {
    std::printf("task : %2d \n", id_);
};


int add(int a, int b) {
    return a + b;
}


int main() {
    std::cout << "Hello, World!" << std::endl;
    ZY::thread_pool pool;
    for (int i = 0; i < 200; i++) {
        pool.push(task_print_id, i);
    }
    pool.submit(task_print_id,1);
    std::vector<std::future<int>> fv;
    for (int i = 0; i < 200; i++) {
        fv.emplace_back(pool.submit(add, i, i));
    }
    for (int i = 0; i < 200; i++) {
        std::cout<<fv[i].get()<<std::endl;
    }
    pool.close();
    return 0;
}
