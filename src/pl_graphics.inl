/*
   pl_graphics.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GRAPHICS_H
#define PL_GRAPHICS_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_DEVICE_ALLOCATION_BLOCK_SIZE 268435456
#define PL_DEVICE_LOCAL_LEVELS 8

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// finalized types
typedef struct _plViewportRegion plViewportRegion;
typedef struct _plCommandBuffer  plCommandBuffer;

// types
typedef struct _plMesh                   plMesh;
typedef struct _plSampler                plSampler;
typedef struct _plRenderPassDesc         plRenderPassDesc;
typedef struct _plGraphicsState          plGraphicsState;
typedef struct _plTextureViewDesc        plTextureViewDesc;
typedef struct _plDeviceMemoryAllocation plDeviceMemoryAllocation;
typedef struct _plDeviceMemoryAllocatorI plDeviceMemoryAllocatorI;
typedef struct _plDeviceAllocationRange  plDeviceAllocationRange;
typedef struct _plDeviceAllocationBlock  plDeviceAllocationBlock;
typedef struct _plDeviceAllocationNode   plDeviceAllocationNode;

// enums
typedef int plBufferBindingType;      // -> enum _plBufferBindingType      // Enum:
typedef int plTextureBindingType;     // -> enum _plTextureBindingType     // Enum:
typedef int plBufferUsage;            // -> enum _plBufferUsage            // Enum:
typedef int plMeshFormatFlags;        // -> enum _plMeshFormatFlags        // Flags:
typedef int plShaderTextureFlags;     // -> enum _plShaderTextureFlags     // Flags:
typedef int plBlendMode;              // -> enum _plBlendMode              // Enum:
typedef int plDepthMode;              // -> enum _plDepthMode              // Enum:
typedef int plStencilMode;            // -> enum _plStencilMode            // Enum:
typedef int plFilter;                 // -> enum _plFilter                 // Enum:
typedef int plWrapMode;               // -> enum _plWrapMode               // Enum:
typedef int plCompareMode;            // -> enum _plCompareMode            // Enum:
typedef int plCullMode;               // -> enum _plCullMode               // Enum:
typedef int plFormat;                 // -> enum _plFormat                 // Enum:
typedef int plStencilOp;              // -> enum _plStencilOp              // Enum:
typedef int plDeviceAllocationStatus; // -> enum  // Enum:

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plViewportRegion
{
    float    fX;
    float    fY;
    float    fWidth;
    float    fHeight;
    float    fMinDepth;
    float    fMaxDepth;
} plViewportRegion;

typedef struct _plRenderPassDesc
{
    plFormat tColorFormat;
    plFormat tDepthFormat;
} plRenderPassDesc;

typedef struct _plTextureViewDesc
{
    plFormat    tFormat; 
    uint32_t    uBaseMip;
    uint32_t    uMips;
    uint32_t    uBaseLayer;
    uint32_t    uLayerCount;
    uint32_t    uSlot;  
} plTextureViewDesc;

typedef struct _plSampler
{
    plFilter      tFilter;
    plCompareMode tCompare;
    plWrapMode    tHorizontalWrap;
    plWrapMode    tVerticalWrap;
    float         fMipBias;
    float         fMinMip;
    float         fMaxMip; 
} plSampler;

typedef struct _plGraphicsState
{
    union 
    {
        struct
        {
            uint64_t ulVertexStreamMask   : 11; // PL_MESH_FORMAT_FLAG_*
            uint64_t ulDepthMode          :  3; // PL_DEPTH_MODE_
            uint64_t ulDepthWriteEnabled  :  1; // bool
            uint64_t ulCullMode           :  2; // PL_CULL_MODE_*
            uint64_t ulBlendMode          :  3; // PL_BLEND_MODE_*
            uint64_t ulShaderTextureFlags :  4; // PL_SHADER_TEXTURE_FLAG_* 
            uint64_t ulStencilMode        :  3; // PL_STENCIL_MODE_*
            uint64_t ulStencilRef         :  8;
            uint64_t ulStencilMask        :  8;
            uint64_t ulStencilOpFail      :  3; // PL_STENCIL_OP_*
            uint64_t ulStencilOpDepthFail :  3; // PL_STENCIL_OP_*
            uint64_t ulStencilOpPass      :  3; // PL_STENCIL_OP_*
            uint64_t _ulUnused            : 12;
        };
        uint64_t ulValue;
    };
    
} plGraphicsState;

typedef struct _plDeviceMemoryAllocation
{
    uint64_t tMemory; // backend specific handle, (i.e. VkDeviceMemory)
    uint64_t ulOffset;
    uint64_t ulSize;
    char*    pHostMapped;
    uint32_t uNodeIndex;
    struct plDeviceMemoryAllocatorO* ptInst;
} plDeviceMemoryAllocation;

typedef struct _plDeviceMemoryAllocatorI
{

    struct plDeviceMemoryAllocatorO* ptInst; // opaque pointer

    plDeviceMemoryAllocation (*allocate)(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
    void                     (*free)    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);
    plDeviceAllocationBlock* (*blocks)  (struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut);
    plDeviceAllocationNode*  (*nodes)   (struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut);
    char**                   (*names)   (struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut);

} plDeviceMemoryAllocatorI;

typedef struct _plDeviceAllocationRange
{
    const char*              pcName;
    plDeviceAllocationStatus tStatus;
    plDeviceMemoryAllocation tAllocation;
} plDeviceAllocationRange;

typedef struct _plDeviceAllocationNode
{
    // static
    uint64_t uNodeIndex;
    uint32_t uMemoryType;
    uint64_t uBlockIndex;
    uint64_t ulSize;
    uint64_t ulOffset;
    
    // dynamic
    uint32_t uNext;
    uint64_t ulSizeWasted; // ulSize if ignored, ulSize + 1 if free, otherwise inuse
} plDeviceAllocationNode;

typedef struct _plDeviceAllocationBlock
{
    uint64_t                 ulAddress;
    uint64_t                 ulSize;
    plDeviceAllocationRange* sbtRanges;
} plDeviceAllocationBlock;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plFormat
{
    PL_FORMAT_UNKNOWN,
    PL_FORMAT_R32G32B32_FLOAT,
    PL_FORMAT_R8G8B8A8_UNORM,
    PL_FORMAT_R32G32_FLOAT,
    PL_FORMAT_R8G8B8A8_SRGB,
    PL_FORMAT_B8G8R8A8_SRGB,
    PL_FORMAT_B8G8R8A8_UNORM,
    PL_FORMAT_D32_FLOAT,
    PL_FORMAT_D32_FLOAT_S8_UINT,
    PL_FORMAT_D24_UNORM_S8_UINT,
    PL_FORMAT_D16_UNORM_S8_UINT
};

enum _plCompareMode
{
    PL_COMPARE_MODE_UNSPECIFIED,
    PL_COMPARE_MODE_NEVER,
    PL_COMPARE_MODE_LESS,
    PL_COMPARE_MODE_EQUAL,
    PL_COMPARE_MODE_LESS_OR_EQUAL,
    PL_COMPARE_MODE_GREATER,
    PL_COMPARE_MODE_NOT_EQUAL,
    PL_COMPARE_MODE_GREATER_OR_EQUAL,
    PL_COMPARE_MODE_ALWAYS
};

enum _plFilter
{
    PL_FILTER_UNSPECIFIED,
    PL_FILTER_NEAREST,
    PL_FILTER_LINEAR
};

enum _plWrapMode
{
    PL_WRAP_MODE_UNSPECIFIED,
    PL_WRAP_MODE_WRAP,
    PL_WRAP_MODE_CLAMP,
    PL_WRAP_MODE_MIRROR
};

enum _plBufferBindingType
{
    PL_BUFFER_BINDING_TYPE_UNSPECIFIED,
    PL_BUFFER_BINDING_TYPE_UNIFORM,
    PL_BUFFER_BINDING_TYPE_STORAGE
};

enum _plTextureBindingType
{
    PL_TEXTURE_BINDING_TYPE_UNSPECIFIED,
    PL_TEXTURE_BINDING_TYPE_SAMPLED,
    PL_TEXTURE_BINDING_TYPE_STORAGE
};

enum _plBufferUsage
{
    PL_BUFFER_USAGE_UNSPECIFIED,
    PL_BUFFER_USAGE_INDEX,
    PL_BUFFER_USAGE_VERTEX,
    PL_BUFFER_USAGE_CONSTANT,
    PL_BUFFER_USAGE_STORAGE
};

enum _plBlendMode
{
    PL_BLEND_MODE_NONE,
    PL_BLEND_MODE_ALPHA,
    PL_BLEND_MODE_ADDITIVE,
    PL_BLEND_MODE_PREMULTIPLY,
    PL_BLEND_MODE_MULTIPLY,
    PL_BLEND_MODE_CLIP_MASK,
    
    PL_BLEND_MODE_COUNT
};

enum _plDepthMode
{
    PL_DEPTH_MODE_NEVER,
    PL_DEPTH_MODE_LESS,
    PL_DEPTH_MODE_EQUAL,
    PL_DEPTH_MODE_LESS_OR_EQUAL,
    PL_DEPTH_MODE_GREATER,
    PL_DEPTH_MODE_NOT_EQUAL,
    PL_DEPTH_MODE_GREATER_OR_EQUAL,
    PL_DEPTH_MODE_ALWAYS,
};

enum _plStencilMode
{
    PL_STENCIL_MODE_NEVER,
    PL_STENCIL_MODE_LESS,
    PL_STENCIL_MODE_EQUAL,
    PL_STENCIL_MODE_LESS_OR_EQUAL,
    PL_STENCIL_MODE_GREATER,
    PL_STENCIL_MODE_NOT_EQUAL,
    PL_STENCIL_MODE_GREATER_OR_EQUAL,
    PL_STENCIL_MODE_ALWAYS,
};

enum _plCullMode
{
    PL_CULL_MODE_NONE  = 0,
    PL_CULL_MODE_FRONT = 1 << 0,
    PL_CULL_MODE_BACK  = 1 << 1,
};

enum _plStencilOp
{
    PL_STENCIL_OP_KEEP,
    PL_STENCIL_OP_ZERO,
    PL_STENCIL_OP_REPLACE,
    PL_STENCIL_OP_INCREMENT_AND_CLAMP,
    PL_STENCIL_OP_DECREMENT_AND_CLAMP,
    PL_STENCIL_OP_INVERT,
    PL_STENCIL_OP_INCREMENT_AND_WRAP,
    PL_STENCIL_OP_DECREMENT_AND_WRAP
};

enum _plMeshFormatFlags
{
    PL_MESH_FORMAT_FLAG_NONE           = 0,
    PL_MESH_FORMAT_FLAG_HAS_POSITION   = 1 << 0,
    PL_MESH_FORMAT_FLAG_HAS_NORMAL     = 1 << 1,
    PL_MESH_FORMAT_FLAG_HAS_TANGENT    = 1 << 2,
    PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 = 1 << 3,
    PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 = 1 << 4,
    PL_MESH_FORMAT_FLAG_HAS_COLOR_0    = 1 << 5,
    PL_MESH_FORMAT_FLAG_HAS_COLOR_1    = 1 << 6,
    PL_MESH_FORMAT_FLAG_HAS_JOINTS_0   = 1 << 7,
    PL_MESH_FORMAT_FLAG_HAS_JOINTS_1   = 1 << 8,
    PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0  = 1 << 9,
    PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1  = 1 << 10
};

enum _plShaderTextureFlags
{
    PL_SHADER_TEXTURE_FLAG_BINDING_NONE       = 0,
    PL_SHADER_TEXTURE_FLAG_BINDING_0          = 1 << 0,
    PL_SHADER_TEXTURE_FLAG_BINDING_1          = 1 << 1,
    PL_SHADER_TEXTURE_FLAG_BINDING_2          = 1 << 2,
    PL_SHADER_TEXTURE_FLAG_BINDING_3          = 1 << 3,
    // PL_SHADER_TEXTURE_FLAG_BINDING_4          = 1 << 4,
    // PL_SHADER_TEXTURE_FLAG_BINDING_5          = 1 << 5,
    // PL_SHADER_TEXTURE_FLAG_BINDING_6          = 1 << 6,
    // PL_SHADER_TEXTURE_FLAG_BINDING_7          = 1 << 7,
    // PL_SHADER_TEXTURE_FLAG_BINDING_8          = 1 << 8,
    // PL_SHADER_TEXTURE_FLAG_BINDING_9          = 1 << 9,
    // PL_SHADER_TEXTURE_FLAG_BINDING_10         = 1 << 10,
    // PL_SHADER_TEXTURE_FLAG_BINDING_11         = 1 << 11,
    // PL_SHADER_TEXTURE_FLAG_BINDING_12         = 1 << 12,
    // PL_SHADER_TEXTURE_FLAG_BINDING_13         = 1 << 13,
    // PL_SHADER_TEXTURE_FLAG_BINDING_14         = 1 << 14
};

enum _plDeviceAllocationStatus
{
    PL_DEVICE_ALLOCATION_STATUS_FREE,
    PL_DEVICE_ALLOCATION_STATUS_USED,
    PL_DEVICE_ALLOCATION_STATUS_WASTE
};

#endif // PL_GRAPHICS_H