#include <iostream>
#include "Adder/adder.h"
#include "glad/glad.h"
#include <GLFW/glfw3.h>

int main() {
    std::cout << "Hello World!\n";
    std::cout << add(71.1f,33.20f) << '\n';

    GLFWwindow* window;
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        exit( EXIT_FAILURE );
    }
    window = glfwCreateWindow( 300, 300, "Gears", NULL, NULL );
    if (!window)
    {
        fprintf( stderr, "Failed to open GLFW window\n" );
        glfwTerminate();
        exit( EXIT_FAILURE );
    }
    glfwMakeContextCurrent(window);

    // init GLAD
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
    {
        fprintf(stderr, "glad initialization failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Main loop
    while( !glfwWindowShouldClose(window) )
    {
        glClearColor(0.3,0.4,0.5,1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Terminate GLFW
    glfwTerminate();

    return 0;
}