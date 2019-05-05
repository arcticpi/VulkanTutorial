#include <iostream>
#include <stdexcept>
#include <cstdlib>

class VulkanApplication
{
public:
	void run()
	{
		InitVulkan();
		MainLoop();
		cleanup();
	}

private:
	void InitVulkan()
	{

	}

	void MainLoop()
	{

	}

	void cleanup()
	{

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