#include <vk_engine.h>

int main(int argc, char* argv[])
{
	VulkanEngine engine({1280, 720});

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}
