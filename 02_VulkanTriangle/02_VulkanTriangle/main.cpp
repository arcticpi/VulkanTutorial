#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

class VulkanApplication
{
public:
	void run()
	{
		InitWindow();
		InitVulkan();
		MainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;
	const int WindowWidth = 800;
	const int WindowHeight = 600;
	const char* WindowTitle = "Vulkan Window";

	void InitWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // we don't need to create an OpenGL context
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WindowWidth, WindowHeight, WindowTitle, nullptr, nullptr);
	}

	void InitVulkan()
	{

	}

	void MainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
		}
	}

	void cleanup()
	{
		glfwDestroyWindow(window);
		glfwTerminate();
	}
};

int main()
{
	VulkanApplication app;

	try
	{
		app.run();
	}
	catch (const std::exception & exc)
	{
		std::cerr << exc.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}