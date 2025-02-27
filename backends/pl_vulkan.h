/*
   pl_vulkan.h
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] public api
// [SECTION] c file
// [SECTION] includes
// [SECTION] shaders
// [SECTION] internal structs
// [SECTION] internal helper forward declarations
// [SECTION] implementation
// [SECTION] internal helpers implementation
*/

#ifndef PL_VULKAN_H
#define PL_VULKAN_H

#if defined(_WIN32)
    #define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
    #define VK_USE_PLATFORM_METAL_EXT
#else // linux
    #define VK_USE_PLATFORM_XCB_KHR
#endif

#define PL_API_VULKAN_DRAW "PL_API_VULKAN_DRAW"

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_draw_ext.h"
#include "vulkan/vulkan.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plVulkanInit
{
   VkPhysicalDevice      tPhysicalDevice;
   VkDevice              tLogicalDevice;
   uint32_t              uImageCount;
   uint32_t              uFramesInFlight;
   VkRenderPass          tRenderPass; // default render pass
   VkSampleCountFlagBits tMSAASampleCount;
} plVulkanInit;

typedef struct _plVulkanDrawApiI
{
   void (*initialize_context)   (const plVulkanInit* ptInit);
   void (*submit_drawlist)      (plDrawList* ptDrawlist, float fWidth, float fHeight, VkCommandBuffer tCmdBuf, uint32_t uFrameIndex);
   void (*submit_drawlist_ex)   (plDrawList* ptDrawlist, float fWidth, float fHeight, VkCommandBuffer tCmdBuf, uint32_t uFrameIndex, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount);
   void (*submit_3d_drawlist)   (plDrawList3D* ptDrawlist, float fWidth, float fHeight, VkCommandBuffer tCmdBuf, uint32_t uFrameIndex, const plMat4* ptMVP, pl3DDrawFlags tFlags);
   void (*submit_3d_drawlist_ex)(plDrawList3D* ptDrawlist, float fWidth, float fHeight, VkCommandBuffer tCmdBuf, uint32_t uFrameIndex, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount, const plMat4* ptMVP, pl3DDrawFlags tFlags);
 
   VkDescriptorSet (*add_texture)(plDrawContext* ptCtx, VkImageView tImageView, VkImageLayout tImageLayout);
} plVulkanDrawApiI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

const plVulkanDrawApiI* pl_load_vulkan_draw_api(void);

#endif // PL_VULKAN_H