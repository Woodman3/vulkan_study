#include <iostream>
#include "GlfwGeneral.hpp"

using namespace std;

int main() {
    if (!InitializeWindow({1280,720}))
        return -1;//来个你讨厌的返回值
    std::cout << std::format("[ InitializeWindow ]\nWindow created successfully!\n");
    while (!glfwWindowShouldClose(pWindow)) {
        /*渲染过程，待填充*/

        glfwPollEvents();
        TitleFps();
    }
    TerminateWindow();
    return 0;
}