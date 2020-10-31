#ifdef __cplusplus
extern "C" {
#endif
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h> 
#include "vulkan/vulkan.h"

#ifdef NDEBUG
	const VkBool32 enableValidationLayers = 0;
#else
	const VkBool32 enableValidationLayers = 1;
#endif
typedef struct {
	VkInstance instance;//a connection between the application and the Vulkan library 
	VkPhysicalDevice physicalDevice;//a handle for the graphics card used in the application
	VkPhysicalDeviceProperties physicalDeviceProperties;//bastic device properties
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;//bastic memory properties of the device
	VkDevice device;//a logical device, interacting with physical device
	VkDebugUtilsMessengerEXT debugMessenger;//extension for debugging
	uint32_t queueFamilyIndex;//if multiple queues are available, specify the used one
	VkQueue queue;//a place, where all operations are submitted
	VkCommandPool commandPool;//an opaque objects that command buffer memory is allocated from
	VkFence fence;//a fence used to synchronize dispatches
	uint32_t device_id;//an id of a device, reported by Vulkan device list
} VkGPU;//an example structure containing Vulkan primitives
typedef struct {
	uint32_t localSize[3];
	uint32_t inputStride[3];
} VkAppSpecializationConstantsLayout;//an example structure on how to set constants in the shader after first compilation but before final shader module creation

typedef struct {
	uint32_t pushID;//an example structure on how to pass small amount of data to the shader right before dispatch
} VkAppPushConstantsLayout;
typedef struct {
	//system size for transposition
	uint32_t size[3];
	//how much memory is coalesced (in bytes) - 32 for Nvidia, 64 for Intel, 64 for AMD. Maximum value: 128
	uint32_t coalescedMemory;
	VkAppSpecializationConstantsLayout specializationConstants;
	VkAppPushConstantsLayout pushConstants;
	//bridging information, that allows shaders to freely access resources like buffers and images
	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	//pipeline used for graphics applications, we only use compute part of it in this example
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	//input buffer
	VkDeviceSize inputBufferSize;//the size of buffer (in bytes)
	VkBuffer* inputBuffer;//pointer to the buffer object
	VkDeviceMemory* inputBufferDeviceMemory;//pointer to the memory object, corresponding to the buffer
	//output buffer
	VkDeviceSize outputBufferSize;
	VkBuffer* outputBuffer;
	VkDeviceMemory* outputBufferDeviceMemory;
} VkApplication;//application specific data


uint32_t* VkFFTReadShader(uint32_t* length, const char* filename) {
	//function that reads shader's SPIR - V bytecode
	FILE* fp = fopen(filename, "rb");
	if (fp == NULL) {
		printf("Could not find or open file: %s\n", filename);
	}

	// get file size.
	fseek(fp, 0, SEEK_END);
	long filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	long filesizepadded = ((long)ceil(filesize / 4.0)) * 4;

	char* str = (char*)malloc(sizeof(char) * filesizepadded);
	fread(str, filesize, sizeof(char), fp);
	fclose(fp);

	for (long i = filesize; i < filesizepadded; i++) {
		str[i] = 0;
	}

	length[0] = filesizepadded;
	return (uint32_t*)str;
}
VkResult CreateDebugUtilsMessengerEXT(VkGPU* vkGPU, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	//pointer to the function, as it is not part of the core. Function creates debugging messenger
	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkGPU->instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != NULL) {
		return func(vkGPU->instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}
void DestroyDebugUtilsMessengerEXT(VkGPU* vkGPU, const VkAllocationCallbacks* pAllocator) {
	//pointer to the function, as it is not part of the core. Function destroys debugging messenger
	PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkGPU->instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != NULL) {
		func(vkGPU->instance, vkGPU->debugMessenger, pAllocator);
	}
}
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	printf("validation layer: %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}
VkResult setupDebugMessenger(VkGPU* vkGPU) {
	//function that sets up the debugging messenger 
	if (enableValidationLayers == 0) return VK_SUCCESS;

	VkDebugUtilsMessengerCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;

	if (CreateDebugUtilsMessengerEXT(vkGPU, &createInfo, NULL, &vkGPU->debugMessenger) != VK_SUCCESS) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	return VK_SUCCESS;
}
VkResult checkValidationLayerSupport() {
	//check if validation layers are supported when an instance is created
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, NULL);

	VkLayerProperties* availableLayers = (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

	for (uint32_t i = 0; i < layerCount; i++) {
		if (strcmp("VK_LAYER_KHRONOS_validation", availableLayers[i].layerName) == 0) {
			free(availableLayers);
			return VK_SUCCESS;
		}
	}
	free(availableLayers);
	return VK_ERROR_LAYER_NOT_PRESENT;
}
VkResult createInstance(VkGPU* vkGPU) {
	//create instance - a connection between the application and the Vulkan library 
	VkResult res = VK_SUCCESS;
	//check if validation layers are supported
	if (enableValidationLayers == 1) {
		res = checkValidationLayerSupport();
		if (res != VK_SUCCESS) return res;
	}
	//sample app information
	VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	applicationInfo.pApplicationName = "VulkanTest";
	applicationInfo.applicationVersion = 1.0;
	applicationInfo.pEngineName = "VulkanTest";
	applicationInfo.engineVersion = 1.0;
	applicationInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	createInfo.flags = 0;
	createInfo.pApplicationInfo = &applicationInfo;
	//specify, whether debugging utils are required
	if (enableValidationLayers == 1) {
		const char* const extensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
		createInfo.enabledExtensionCount = 1;
		createInfo.ppEnabledExtensionNames = &extensions;
	}
	else {
		createInfo.enabledExtensionCount = 0;
		createInfo.ppEnabledExtensionNames = NULL;
	}
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	if (enableValidationLayers == 1) {
		//query for the validation layer support in the instance
		createInfo.enabledLayerCount = 1;
		const char* validationLayers = "VK_LAYER_KHRONOS_validation";
		createInfo.ppEnabledLayerNames = &validationLayers;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = debugCallback;
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = NULL;
	}
	//create instance
	res = vkCreateInstance(&createInfo, NULL, &vkGPU->instance);
	if (res != VK_SUCCESS) return res;
	
	return res;
}
VkResult findPhysicalDevice(VkGPU* vkGPU) {
	//check if there are GPUs that support Vulkan and select one
	VkResult res = VK_SUCCESS;
	uint32_t deviceCount;
	res = vkEnumeratePhysicalDevices(vkGPU->instance, &deviceCount, NULL);
	if (res != VK_SUCCESS) return res;
	if (deviceCount == 0) {
		return VK_ERROR_DEVICE_LOST;
	}

	VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * deviceCount);
	res = vkEnumeratePhysicalDevices(vkGPU->instance, &deviceCount, devices);
	if (res != VK_SUCCESS) return res;
	vkGPU->physicalDevice = devices[vkGPU->device_id];
	free(devices);
	return VK_SUCCESS;
}
VkResult getComputeQueueFamilyIndex(VkGPU* vkGPU) {
	//find a queue family for a selected GPU, select the first available for use
	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(vkGPU->physicalDevice, &queueFamilyCount, NULL);

	VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(vkGPU->physicalDevice, &queueFamilyCount, queueFamilies);
	uint32_t i = 0;
	for (; i < queueFamilyCount; i++) {
		VkQueueFamilyProperties props = queueFamilies[i];

		if (props.queueCount > 0 && (props.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
			break;
		}
	}
	free(queueFamilies);
	if (i == queueFamilyCount) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	vkGPU->queueFamilyIndex = i;
	return VK_SUCCESS;
}
VkResult createDevice(VkGPU* vkGPU) {
	//create logical device representation
	VkResult res = VK_SUCCESS;
	VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	res = getComputeQueueFamilyIndex(vkGPU);
	if (res != VK_SUCCESS) return res;
	queueCreateInfo.queueFamilyIndex = vkGPU->queueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	float queuePriorities = 1.0;
	queueCreateInfo.pQueuePriorities = &queuePriorities;
	VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	VkPhysicalDeviceFeatures deviceFeatures = { 0 };
	deviceFeatures.shaderFloat64 = VK_TRUE;//this enables double precision support in shaders 
	deviceCreateInfo.enabledExtensionCount = 0;
	deviceCreateInfo.ppEnabledExtensionNames = NULL;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pEnabledFeatures = NULL;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	res = vkCreateDevice(vkGPU->physicalDevice, &deviceCreateInfo, NULL, &vkGPU->device);
	if (res != VK_SUCCESS) return res;
	vkGetDeviceQueue(vkGPU->device, vkGPU->queueFamilyIndex, 0, &vkGPU->queue);
	return res;
}
VkResult createFence(VkGPU* vkGPU) {
	//create fence for synchronization 
	VkResult res = VK_SUCCESS;
	VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceCreateInfo.flags = 0;
	res = vkCreateFence(vkGPU->device, &fenceCreateInfo, NULL, &vkGPU->fence);
	return res;
}
VkResult createCommandPool(VkGPU* vkGPU) {
	//create a place, command buffer memory is allocated from
	VkResult res = VK_SUCCESS;
	VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = vkGPU->queueFamilyIndex;
	res = vkCreateCommandPool(vkGPU->device, &commandPoolCreateInfo, NULL, &vkGPU->commandPool);
	return res;
}
VkResult createShaderModule(VkGPU* vkGPU, VkShaderModule* shaderModule, uint32_t shaderID) {
	//create shader module, using the SPIR-V bytecode
	VkResult res = VK_SUCCESS;
	char shaderPath[256];
	//this sample uses two compute shaders, that can be selected by passing an appropriate id
	switch (shaderID) {
	case 0:
		sprintf(shaderPath, "%stransposition_no_bank_conflicts.spv", SHADER_DIR);
		break;
	case 1:
		sprintf(shaderPath, "%stransposition_bank_conflicts.spv", SHADER_DIR);
		break;
	case 2:
		sprintf(shaderPath, "%stransfer.spv", SHADER_DIR);
		break;
	default:
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	uint32_t filelength;
	//read bytecode
	uint32_t* code = VkFFTReadShader(&filelength, shaderPath);
	VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	createInfo.pCode = code;
	createInfo.codeSize = filelength;
	res = vkCreateShaderModule(vkGPU->device, &createInfo, NULL, shaderModule);
	free(code);
	return res;
}
VkResult createApp(VkGPU* vkGPU, VkApplication* app, uint32_t shaderID) {
	//create an application interface to Vulkan. This function binds the shader to the compute pipeline, so it can be used as a part of the command buffer later
	VkResult res = VK_SUCCESS;
	//we have two storage buffer objects in one set in one pool
	VkDescriptorPoolSize descriptorPoolSize = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
	descriptorPoolSize.descriptorCount = 2;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
	descriptorPoolCreateInfo.maxSets = 1;
	res = vkCreateDescriptorPool(vkGPU->device, &descriptorPoolCreateInfo, NULL, &app->descriptorPool);
	if (res != VK_SUCCESS) return res;
	//specify each object from the set as a storage buffer
	const VkDescriptorType descriptorType[2] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
	VkDescriptorSetLayoutBinding* descriptorSetLayoutBindings = (VkDescriptorSetLayoutBinding*)malloc(descriptorPoolSize.descriptorCount * sizeof(VkDescriptorSetLayoutBinding));
	for (uint32_t i = 0; i < descriptorPoolSize.descriptorCount; ++i) {
		descriptorSetLayoutBindings[i].binding = i;
		descriptorSetLayoutBindings[i].descriptorType = descriptorType[i];
		descriptorSetLayoutBindings[i].descriptorCount = 1;
		descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	descriptorSetLayoutCreateInfo.bindingCount = descriptorPoolSize.descriptorCount;
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;
	//create layout
	res = vkCreateDescriptorSetLayout(vkGPU->device, &descriptorSetLayoutCreateInfo, NULL, &app->descriptorSetLayout);
	if (res != VK_SUCCESS) return res;
	free(descriptorSetLayoutBindings);
	//provide the layout with actual buffers and their sizes
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	descriptorSetAllocateInfo.descriptorPool = app->descriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &app->descriptorSetLayout;
	res = vkAllocateDescriptorSets(vkGPU->device, &descriptorSetAllocateInfo, &app->descriptorSet);
	if (res != VK_SUCCESS) return res;
	for (uint32_t i = 0; i < descriptorPoolSize.descriptorCount; ++i) {


		VkDescriptorBufferInfo descriptorBufferInfo = { 0 };
		if (i == 0) {
			descriptorBufferInfo.buffer = app->inputBuffer[0];
			descriptorBufferInfo.range = app->inputBufferSize;
			descriptorBufferInfo.offset = 0;
		}
		if (i == 1) {
			descriptorBufferInfo.buffer = app->outputBuffer[0];
			descriptorBufferInfo.range = app->outputBufferSize;
			descriptorBufferInfo.offset = 0;
		}

		VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writeDescriptorSet.dstSet = app->descriptorSet;
		writeDescriptorSet.dstBinding = i;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = descriptorType[i];
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
		vkUpdateDescriptorSets(vkGPU->device, 1, &writeDescriptorSet, 0, NULL);
	}



	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &app->descriptorSetLayout;
	//specify how many push constants can be specified when the pipeline is bound to the command buffer
	VkPushConstantRange pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT };
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(VkAppPushConstantsLayout);
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	//create pipeline layout
	res = vkCreatePipelineLayout(vkGPU->device, &pipelineLayoutCreateInfo, NULL, &app->pipelineLayout);
	if (res != VK_SUCCESS) return res;
	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };

	VkComputePipelineCreateInfo computePipelineCreateInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	//specify specialization constants - structure that sets constants in the shader after first compilation (done by glslangvalidator, for example) but before final shader module creation
	//first three values - workgroup dimensions 
	app->specializationConstants.localSize[0] = app->coalescedMemory / sizeof(float);
	app->specializationConstants.localSize[1] = app->coalescedMemory / sizeof(float);
	app->specializationConstants.localSize[2] = 1;
	//next three - buffer strides for multidimensional data
	app->specializationConstants.inputStride[0] = 1;
	app->specializationConstants.inputStride[1] = app->size[0];
	app->specializationConstants.inputStride[2] = app->size[0]*app->size[1];

	VkSpecializationMapEntry specializationMapEntries[6] = { 0 };
	for (uint32_t i = 0; i < 6; i++) {
		specializationMapEntries[i].constantID = i + 1;
		specializationMapEntries[i].size = sizeof(uint32_t);
		specializationMapEntries[i].offset = i * sizeof(uint32_t);
	}
	VkSpecializationInfo specializationInfo = { 0 };
	specializationInfo.dataSize = 6 * sizeof(uint32_t);
	specializationInfo.mapEntryCount = 6;
	specializationInfo.pMapEntries = specializationMapEntries;
	specializationInfo.pData = &app->specializationConstants;

	pipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	//create a shader module from the byte code
	res = createShaderModule(vkGPU, &pipelineShaderStageCreateInfo.module, shaderID);
	if (res != VK_SUCCESS) return res;
	pipelineShaderStageCreateInfo.pSpecializationInfo = &specializationInfo;
	pipelineShaderStageCreateInfo.pName = "main";
	computePipelineCreateInfo.stage = pipelineShaderStageCreateInfo;
	computePipelineCreateInfo.layout = app->pipelineLayout;
	//create pipeline
	res = vkCreateComputePipelines(vkGPU->device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, NULL, &app->pipeline);
	if (res != VK_SUCCESS) return res;
	vkDestroyShaderModule(vkGPU->device, pipelineShaderStageCreateInfo.module, NULL);
	return res;
}
void appendApp(VkGPU* vkGPU, VkApplication* app, VkCommandBuffer* commandBuffer) {
	//this function appends to the command buffer: push constants, binds pipeline, descriptors, the shader's program dispatch call and the barrier between two compute stages to avoid race conditions 
	VkMemoryBarrier memory_barrier = {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				0,
				VK_ACCESS_SHADER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,
	};
	app->pushConstants.pushID = 0;
	//specify push constants - small amount of constant data in the shader
	vkCmdPushConstants(commandBuffer[0], app->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VkAppPushConstantsLayout), &app->pushConstants);
	//bind compute pipeline to the command buffer
	vkCmdBindPipeline(commandBuffer[0], VK_PIPELINE_BIND_POINT_COMPUTE, app->pipeline);
	//bind descriptors to the command buffer
	vkCmdBindDescriptorSets(commandBuffer[0], VK_PIPELINE_BIND_POINT_COMPUTE, app->pipelineLayout, 0, 1, &app->descriptorSet, 0, NULL);
	//record dispatch call to the command buffer - specifies the total amount of workgroups
	vkCmdDispatch(commandBuffer[0], app->size[0] / app->specializationConstants.localSize[0], app->size[1] / app->specializationConstants.localSize[1], app->size[2] / app->specializationConstants.localSize[2]);
	//memory synchronization between two compute dispatches
	vkCmdPipelineBarrier(commandBuffer[0], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memory_barrier, 0, NULL, 0, NULL);

}
VkResult runApp(VkGPU* vkGPU, VkApplication* app, uint32_t batch, double* time) {
	VkResult res = VK_SUCCESS;
	//create command buffer to be executed on the GPU
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	commandBufferAllocateInfo.commandPool = vkGPU->commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer = {0};
	res = vkAllocateCommandBuffers(vkGPU->device, &commandBufferAllocateInfo, &commandBuffer);
	VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	//begin command buffer recording
	res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	if (res != VK_SUCCESS) return res;
	//Record commands batch times. Allows to perform multiple operations in one submit to mitigate dispatch overhead
	for (uint32_t i = 0; i < batch; i++) {
		appendApp(vkGPU, app, &commandBuffer);
	}
	//end command buffer recording
	res = vkEndCommandBuffer(commandBuffer);
	if (res != VK_SUCCESS) return res;
	//submit the command buffer for execution and place the fence after, measure time required for execution
	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	clock_t t;
	t = clock();
	res = vkQueueSubmit(vkGPU->queue, 1, &submitInfo, vkGPU->fence);
	if (res != VK_SUCCESS) return res;
	res = vkWaitForFences(vkGPU->device, 1, &vkGPU->fence, VK_TRUE, 100000000000);
	if (res != VK_SUCCESS) return res;
	t = clock() - t;
	time[0] = ((double)t) / CLOCKS_PER_SEC * 1000/batch; //in ms
	res = vkResetFences(vkGPU->device, 1, &vkGPU->fence);
	if (res != VK_SUCCESS) return res;
	//free the command buffer
	vkFreeCommandBuffers(vkGPU->device, vkGPU->commandPool, 1, &commandBuffer);
	return res;
}
void deleteApp(VkGPU* vkGPU, VkApplication* app) {
	//destroy previously allocated resources of the application
	vkDestroyDescriptorPool(vkGPU->device, app->descriptorPool, NULL);
	vkDestroyDescriptorSetLayout(vkGPU->device, app->descriptorSetLayout, NULL);
	vkDestroyPipelineLayout(vkGPU->device, app->pipelineLayout, NULL);
	vkDestroyPipeline(vkGPU->device, app->pipeline, NULL);
}
VkResult findMemoryType(VkGPU* vkGPU, uint32_t memoryTypeBits, VkMemoryPropertyFlags properties, uint32_t* memoryTypeIndex) {
	//find memory with specified properties
	VkPhysicalDeviceMemoryProperties memoryProperties = { 0 };

	vkGetPhysicalDeviceMemoryProperties(vkGPU->physicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
		if ((memoryTypeBits & (1 << i)) && ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties))
		{
			memoryTypeIndex[0] = i;
			return VK_SUCCESS;
		}
	}
	return VK_ERROR_INITIALIZATION_FAILED;
}
VkResult allocateFFTBuffer(VkGPU* vkGPU, VkBuffer* buffer, VkDeviceMemory* deviceMemory, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkDeviceSize size) {
	//allocate the buffer used by the GPU with specified properties
	VkResult res = VK_SUCCESS;
	uint32_t queueFamilyIndices;
	VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 1;
	bufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndices;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usageFlags;
	res = vkCreateBuffer(vkGPU->device, &bufferCreateInfo, NULL, buffer);
	if (res != VK_SUCCESS) return res;
	VkMemoryRequirements memoryRequirements = { 0 };
	vkGetBufferMemoryRequirements(vkGPU->device, buffer[0], &memoryRequirements);
	VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	res = findMemoryType(vkGPU, memoryRequirements.memoryTypeBits, propertyFlags, &memoryAllocateInfo.memoryTypeIndex);
	if (res != VK_SUCCESS) return res;
	res = vkAllocateMemory(vkGPU->device, &memoryAllocateInfo, NULL, deviceMemory);
	if (res != VK_SUCCESS) return res;
	res = vkBindBufferMemory(vkGPU->device, buffer[0], deviceMemory[0], 0);
	if (res != VK_SUCCESS) return res;
	return res;
}
VkResult transferDataFromCPU(VkGPU* vkGPU, void* arr, VkBuffer* buffer, VkDeviceSize bufferSize) {
	//a function that transfers data from the CPU to the GPU using staging buffer, because the GPU memory is not host-coherent
	VkResult res = VK_SUCCESS;
	VkDeviceSize stagingBufferSize = bufferSize;
	VkBuffer stagingBuffer = { 0 };
	VkDeviceMemory stagingBufferMemory = { 0 };
	res = allocateFFTBuffer(vkGPU, &stagingBuffer, &stagingBufferMemory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferSize);
	if (res != VK_SUCCESS) return res;
	void* data;
	res = vkMapMemory(vkGPU->device, stagingBufferMemory, 0, stagingBufferSize, 0, &data);
	if (res != VK_SUCCESS) return res;
	memcpy(data, arr, stagingBufferSize);
	vkUnmapMemory(vkGPU->device, stagingBufferMemory);
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	commandBufferAllocateInfo.commandPool = vkGPU->commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer = { 0 };
	res = vkAllocateCommandBuffers(vkGPU->device, &commandBufferAllocateInfo, &commandBuffer);
	if (res != VK_SUCCESS) return res;
	VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	if (res != VK_SUCCESS) return res;
	VkBufferCopy copyRegion = { 0 };
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = stagingBufferSize;
	vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer[0], 1, &copyRegion);
	res = vkEndCommandBuffer(commandBuffer);
	if (res != VK_SUCCESS) return res;
	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	res = vkQueueSubmit(vkGPU->queue, 1, &submitInfo, vkGPU->fence);
	if (res != VK_SUCCESS) return res;
	res = vkWaitForFences(vkGPU->device, 1, &vkGPU->fence, VK_TRUE, 100000000000);
	if (res != VK_SUCCESS) return res;
	res = vkResetFences(vkGPU->device, 1, &vkGPU->fence);
	if (res != VK_SUCCESS) return res;
	vkFreeCommandBuffers(vkGPU->device, vkGPU->commandPool, 1, &commandBuffer);
	vkDestroyBuffer(vkGPU->device, stagingBuffer, NULL);
	vkFreeMemory(vkGPU->device, stagingBufferMemory, NULL);
	return res;
}
VkResult transferDataToCPU(VkGPU* vkGPU, void* arr, VkBuffer* buffer, VkDeviceSize bufferSize) {
	//a function that transfers data from the GPU to the CPU using staging buffer, because the GPU memory is not host-coherent
	VkResult res = VK_SUCCESS;
	VkDeviceSize stagingBufferSize = bufferSize;
	VkBuffer stagingBuffer = { 0 };
	VkDeviceMemory stagingBufferMemory = { 0 };
	res = allocateFFTBuffer(vkGPU, &stagingBuffer, &stagingBufferMemory, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferSize);
	if (res != VK_SUCCESS) return res;
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	commandBufferAllocateInfo.commandPool = vkGPU->commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer = { 0 };
	res = vkAllocateCommandBuffers(vkGPU->device, &commandBufferAllocateInfo, &commandBuffer);
	if (res != VK_SUCCESS) return res;
	VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	if (res != VK_SUCCESS) return res;
	VkBufferCopy copyRegion = { 0 };
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = stagingBufferSize;
	vkCmdCopyBuffer(commandBuffer, buffer[0], stagingBuffer, 1, &copyRegion);
	vkEndCommandBuffer(commandBuffer);
	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	res = vkQueueSubmit(vkGPU->queue, 1, &submitInfo, vkGPU->fence);
	if (res != VK_SUCCESS) return res;
	res = vkWaitForFences(vkGPU->device, 1, &vkGPU->fence, VK_TRUE, 100000000000);
	if (res != VK_SUCCESS) return res;
	res = vkResetFences(vkGPU->device, 1, &vkGPU->fence);
	if (res != VK_SUCCESS) return res;
	vkFreeCommandBuffers(vkGPU->device, vkGPU->commandPool, 1, &commandBuffer);
	void* data;
	res = vkMapMemory(vkGPU->device, stagingBufferMemory, 0, stagingBufferSize, 0, &data);
	if (res != VK_SUCCESS) return res;
	memcpy(arr, data, stagingBufferSize);
	vkUnmapMemory(vkGPU->device, stagingBufferMemory);
	vkDestroyBuffer(vkGPU->device, stagingBuffer, NULL);
	vkFreeMemory(vkGPU->device, stagingBufferMemory, NULL);
}

VkResult devices_list() {
	//this function creates an instance and prints the list of available devices
	VkResult res = VK_SUCCESS;
	VkInstance local_instance = {0};
	VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	createInfo.flags = 0;
	createInfo.pApplicationInfo = NULL;
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	createInfo.enabledLayerCount = 0;
	createInfo.enabledExtensionCount = 0;
	createInfo.pNext = NULL;
	res = vkCreateInstance(&createInfo, NULL, &local_instance);
	if (res != VK_SUCCESS) return res;

	uint32_t deviceCount;
	res = vkEnumeratePhysicalDevices(local_instance, &deviceCount, NULL);
	if (res != VK_SUCCESS) return res;

	VkPhysicalDevice* devices=(VkPhysicalDevice *) malloc(sizeof(VkPhysicalDevice)*deviceCount);
	res = vkEnumeratePhysicalDevices(local_instance, &deviceCount, devices);
	if (res != VK_SUCCESS) return res;
	for (uint32_t i = 0; i < deviceCount; i++) {
		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(devices[i], &device_properties);
		printf("Device id: %d name: %s API:%d.%d.%d\n", i, device_properties.deviceName, (device_properties.apiVersion >> 22), ((device_properties.apiVersion >> 12) & 0x3ff), (device_properties.apiVersion & 0xfff));
	}
	free(devices);
	vkDestroyInstance(local_instance, NULL);
	return res;
}

VkResult VulkanTest(uint32_t deviceID, uint32_t coalescedMemory, uint32_t size)
{
	VkGPU vkGPU = { 0 };
	vkGPU.device_id = deviceID;
	VkResult res = VK_SUCCESS;
	//create instance - a connection between the application and the Vulkan library 
	res = createInstance(&vkGPU);
	if (res != VK_SUCCESS) {
		printf("Instance creation failed, error code: %d\n", res);
		return res;
	}
	//set up the debugging messenger 
	res = setupDebugMessenger(&vkGPU);
	if (res != VK_SUCCESS) {
		printf("Debug messenger creation failed, error code: %d\n", res);
		return res;
	}
	//check if there are GPUs that support Vulkan and select one
	res = findPhysicalDevice(&vkGPU);
	if (res != VK_SUCCESS) {
		printf("Physical device not found, error code: %d\n", res);
		return res;
	}
	//create logical device representation
	res = createDevice(&vkGPU);
	if (res != VK_SUCCESS) {
		printf("Device creation failed, error code: %d\n", res);
		return res;
	}
	//create fence for synchronization 
	res = createFence(&vkGPU);
	if (res != VK_SUCCESS) {
		printf("Fence creation failed, error code: %d\n", res);
		return res;
	}
	//create a place, command buffer memory is allocated from
	res = createCommandPool(&vkGPU);
	if (res != VK_SUCCESS) {
		printf("Fence creation failed, error code: %d\n", res);
		return res;
	}
	//get device properties and memory properties, if needed
	vkGetPhysicalDeviceProperties(vkGPU.physicalDevice, &vkGPU.physicalDeviceProperties);
	vkGetPhysicalDeviceMemoryProperties(vkGPU.physicalDevice, &vkGPU.physicalDeviceMemoryProperties);
	//create app template and set the system size, the amount of memory to coalesce
	VkApplication app = { 0 };
	app.size[0] = size;
	app.size[1] = size;
	app.size[2] = 1;
	//use default values if coalescedMemory = 0
	if (coalescedMemory == 0) {
		switch (vkGPU.physicalDeviceProperties.vendorID) {
		case 0x10DE://NVIDIA - change to 128 before Pascal
			app.coalescedMemory = 32;
			break;
		case 0x8086://INTEL
			app.coalescedMemory = 64;
			break;
		case 0x13B5://AMD
			app.coalescedMemory = 64;
			break;
		default:
			app.coalescedMemory = 64;
			break;
		}
	}
	else
		app.coalescedMemory = coalescedMemory;
	//allocate input and output buffers
	VkDeviceSize inputBufferSize=sizeof(float)* app.size[0] * app.size[1] * app.size[2];
	VkBuffer inputBuffer = { 0 };
	VkDeviceMemory inputBufferDeviceMemory = { 0 };

	VkDeviceSize outputBufferSize=sizeof(float) * app.size[0] * app.size[1] * app.size[2];
	VkBuffer outputBuffer = { 0 };
	VkDeviceMemory outputBufferDeviceMemory = { 0 };

	res = allocateFFTBuffer(&vkGPU, &inputBuffer, &inputBufferDeviceMemory, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, inputBufferSize);
	if (res != VK_SUCCESS) {
		printf("Input buffer allocation failed, error code: %d\n", res);
		return res;
	}
	res = allocateFFTBuffer(&vkGPU, &outputBuffer, &outputBufferDeviceMemory, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, outputBufferSize);
	if (res != VK_SUCCESS) {
		printf("Output buffer allocation failed, error code: %d\n", res);
		return res;
	}
	//specify pointers in the app with the previously allocated buffers data
	app.inputBufferSize = inputBufferSize;
	app.inputBuffer = &inputBuffer;
	app.inputBufferDeviceMemory = &inputBufferDeviceMemory;
	app.outputBufferSize = outputBufferSize;
	app.outputBuffer = &outputBuffer;
	app.outputBufferDeviceMemory = &outputBufferDeviceMemory;
	//copy app for bank conflicted shared memory sample and bandwidth sample
	VkApplication app_bank_conflicts = app;
	VkApplication app_bandwidth = app;
	//create transposition app with no bank conflicts from transposition shader
	res = createApp(&vkGPU, &app, 0);
	if (res != VK_SUCCESS) {
		printf("Application creation failed, error code: %d\n", res);
		return res;
	}
	//create transposition app with bank conflicts from transposition shader
	res = createApp(&vkGPU, &app_bank_conflicts, 1);
	if (res != VK_SUCCESS) {
		printf("Application creation failed, error code: %d\n", res);
		return res;
	}
	//create bandwidth app, from the shader with only data transfers and no transosition
	res = createApp(&vkGPU, &app_bandwidth, 2);
	if (res != VK_SUCCESS) {
		printf("Application creation failed, error code: %d\n", res);
		return res;
	}
	//allocate input data on the CPU
	float* buffer_input = (float*)malloc(inputBufferSize);

	for (uint32_t k = 0; k < app.size[2]; k++) {
		for (uint32_t j = 0; j < app.size[1]; j++) {
			for (uint32_t i = 0; i < app.size[0]; i++) {
				buffer_input[ (i + j * app.size[0] + k * (app.size[0]) * app.size[1])] = (i + j * app.size[0] + k * (app.size[0]) * app.size[1]);
			}
		}
	}
	//transfer data to the GPU
	transferDataFromCPU(&vkGPU,buffer_input, &inputBuffer, inputBufferSize);
	free(buffer_input);
	double time_no_bank_conflicts = 0;
	double time_bank_conflicts = 0;
	double time_bandwidth = 0;
	//perform transposition with no bank conflicts on the input buffer and store it in the output 1000 times
	res = runApp(&vkGPU, &app, 1000, &time_no_bank_conflicts);
	if (res != VK_SUCCESS) {
		printf("Application 0 run failed, error code: %d\n", res);
		return res;
	}
	float* buffer_output = (float*)malloc(outputBufferSize);

	//Transfer data from GPU using staging buffer, if needed
	transferDataToCPU(&vkGPU, buffer_output, &outputBuffer, outputBufferSize);
	//Print data, if needed.
	/*for (uint32_t k = 0; k < app.size[2]; k++) {
		for (uint32_t j = 0; j < app.size[1]; j++) {
			for (uint32_t i = 0; i < app.size[0]; i++) {
				printf("%.6f ", buffer_output[i + j * app.size[0] + k * (app.size[0] * app.size[1])]);
			}
			printf("\n");
		}
		printf("\n");
	}*/
	//perform transposition with bank conflicts on the input buffer and store it in the output 1000 times
	res = runApp(&vkGPU, &app_bank_conflicts, 1000, &time_bank_conflicts);
	if (res != VK_SUCCESS) {
		printf("Application 1 run failed, error code: %d\n", res);
		return res;
	}
	//transfer data from the input buffer to the output buffer 1000 times
	res = runApp(&vkGPU, &app_bandwidth, 1000, &time_bandwidth);
	if (res != VK_SUCCESS) {
		printf("Application 2 run failed, error code: %d\n", res);
		return res;
	}
	//print results
	printf("Transpose time with no bank conflicts: %.3f ms\nTranspose time with bank conflicts: %.3f ms\nTransfer time: %.3f ms\nCoalesced Memory: %d bytes\nSystem size: %dx%d\nBuffer size: %d KB\nBandwidth: %d GB/s\nTranfer time/total transpose time: %0.3f%%\n", time_no_bank_conflicts, time_bank_conflicts, time_bandwidth, app.coalescedMemory, app.size[0], app.size[1],inputBufferSize / 1024, (int)(2*1000*inputBufferSize / 1024.0 / 1024.0 / 1024.0 /time_bandwidth), time_bandwidth/ time_no_bank_conflicts *100);
	
	//free resources
	free(buffer_output);
	vkDestroyBuffer(vkGPU.device, inputBuffer, NULL);
	vkFreeMemory(vkGPU.device, inputBufferDeviceMemory, NULL);
	vkDestroyBuffer(vkGPU.device, outputBuffer, NULL);
	vkFreeMemory(vkGPU.device, outputBufferDeviceMemory, NULL);
	deleteApp(&vkGPU, &app);
	deleteApp(&vkGPU, &app_bank_conflicts);
	deleteApp(&vkGPU, &app_bandwidth);
	vkDestroyFence(vkGPU.device, vkGPU.fence, NULL);
	vkDestroyCommandPool(vkGPU.device, vkGPU.commandPool, NULL);
	vkDestroyDevice(vkGPU.device, NULL);
	DestroyDebugUtilsMessengerEXT(&vkGPU, NULL);
	vkDestroyInstance(vkGPU.instance, NULL);
}
int findFlag(char** argv, int num, char* flag) {
	//search for the flag in argv
	for (int i = 0; i < num; i++) {
		if (strstr(argv[i], flag) != NULL) return i;
	}
	return 0;
}
int main(int argc, char* argv[])
{
	uint32_t device_id = 0;//device id used in application
	uint32_t coalescedMemory = 0;//how much memory is coalesced
	uint32_t size = 2048;
	
	//sample CLI
	if (findFlag(argv, argc, "-h")>0)
	{
		//print help
		printf("Vulkan Compute transposition sample v1.0.0 (31-10-2020). Author: Dmitrii Tolmachev\n");
		printf("	-h: print help\n");
		printf("	-devices: print the list of available GPU devices\n");
		printf("	-d X: select GPU device (default 0)\n");
		printf("	-c X: specify how much memory is coalesced per transfer: 4-128 bytes (default Nvidia: 32, Intel/AMD: 64, default: 64)\n");
		printf("	-size X: specify square array size: should be >= coalescedMemory (default 2048)\n");
		return 0;
	}
	if (findFlag(argv, argc, "-devices")>0)
	{
		//print device list
		devices_list();
		return 0;
	}
	if (findFlag(argv, argc, "-d")>0)
	{
		//select device_id
		char* value = argv[findFlag(argv, argc, "-d") + 1];
		if (findFlag(argv, argc, "-d") + 1 != argc) {
			int res = sscanf(value, "%d", &device_id);
			if (res == 0) {
				printf("No device is selected with -d flag\n");
				return 1;
			}
		}
		else {
			printf("No device is selected with -d flag\n");
			return 1;
		}
	}
	if (findFlag(argv, argc, "-c") > 0)
	{
		//select how much memory is coalesced
		char* value = argv[findFlag(argv, argc, "-c") + 1];
		if (findFlag(argv, argc, "-c") + 1 != argc) {
			int res = sscanf(value, "%d", &coalescedMemory);
			if (res == 0) {
				printf("No coalescedMemory constant is selected with -c flag\n");
				return 1;
			}
		}
		else {
			printf("No coalescedMemory constant is selected with -c flag\n");
			return 1;
		}
	}
	if (findFlag(argv, argc, "-size") > 0)
	{
		//select the square array size for transposition 
		char* value = argv[findFlag(argv, argc, "-size") + 1];
		if (findFlag(argv, argc, "-size") + 1 != argc) {
			int res = sscanf(value, "%d", &size);
			if (size < coalescedMemory) {
				printf("Array size should be bigger than coalescedMemory constant\n");
				return 1;
			}
			if (res == 0) {
				printf("No array size is selected with -size flag\n");
				return 1;
			}
		}
		else {
			printf("No array size is selected with -size flag\n");
			return 1;
		}
	}
	VkResult res = VulkanTest(device_id, coalescedMemory, size);
	return res;
}
#ifdef __cplusplus
}
#endif