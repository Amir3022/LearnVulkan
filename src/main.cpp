#include <vk_engine.h>

int main(int argc, char* argv[])
{
	VulkanEngine engine({800, 600});

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}
