//based on https://vkguide.dev/ by Victor Blanco
#include "vk_engine.h"

int main() {
	VulkanEngine engine;
	engine.init();
	engine.run();
	engine.cleanup();
	return 0;
}