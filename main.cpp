#include <iostream>
#include <thread>

void hello(){
    std::cout << "Hello Parallel World!" << std::endl;
}

int main(){
    std::thread thread(hello);
    thread.join();
    return 0;
}
