// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"

#include <dawn/dawn.h>

#include "third_party/blink/renderer/modules/webgpu/gpu_color.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_extent_3d.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_origin_3d.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_stage_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

template <>
DawnBindingType AsDawnEnum<DawnBindingType>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "sampler") {
    return DAWN_BINDING_TYPE_SAMPLER;
  }
  if (webgpu_enum == "sampled-texture") {
    return DAWN_BINDING_TYPE_SAMPLED_TEXTURE;
  }
  if (webgpu_enum == "storage-buffer") {
    return DAWN_BINDING_TYPE_STORAGE_BUFFER;
  }
  if (webgpu_enum == "uniform-buffer") {
    return DAWN_BINDING_TYPE_UNIFORM_BUFFER;
  }
  NOTREACHED();
  return DAWN_BINDING_TYPE_FORCE32;
}

template <>
DawnCompareFunction AsDawnEnum<DawnCompareFunction>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "never") {
    return DAWN_COMPARE_FUNCTION_NEVER;
  }
  if (webgpu_enum == "less") {
    return DAWN_COMPARE_FUNCTION_LESS;
  }
  if (webgpu_enum == "equal") {
    return DAWN_COMPARE_FUNCTION_EQUAL;
  }
  if (webgpu_enum == "less-equal") {
    return DAWN_COMPARE_FUNCTION_LESS_EQUAL;
  }
  if (webgpu_enum == "greater") {
    return DAWN_COMPARE_FUNCTION_GREATER;
  }
  if (webgpu_enum == "not-equal") {
    return DAWN_COMPARE_FUNCTION_NOT_EQUAL;
  }
  if (webgpu_enum == "greater-equal") {
    return DAWN_COMPARE_FUNCTION_GREATER_EQUAL;
  }
  if (webgpu_enum == "always") {
    return DAWN_COMPARE_FUNCTION_ALWAYS;
  }
  NOTREACHED();
  return DAWN_COMPARE_FUNCTION_FORCE32;
}

template <>
DawnTextureFormat AsDawnEnum<DawnTextureFormat>(
    const WTF::String& webgpu_enum) {
  // Normal 8 bit formats
  if (webgpu_enum == "r8unorm") {
    return DAWN_TEXTURE_FORMAT_R8_UNORM;
  }
  if (webgpu_enum == "r8snorm") {
    return DAWN_TEXTURE_FORMAT_R8_SNORM;
  }
  if (webgpu_enum == "r8uint") {
    return DAWN_TEXTURE_FORMAT_R8_UINT;
  }
  if (webgpu_enum == "r8sint") {
    return DAWN_TEXTURE_FORMAT_R8_SINT;
  }

  // Normal 16 bit formats
  if (webgpu_enum == "r16unorm") {
    return DAWN_TEXTURE_FORMAT_R16_UNORM;
  }
  if (webgpu_enum == "r16snorm") {
    return DAWN_TEXTURE_FORMAT_R16_SNORM;
  }
  if (webgpu_enum == "r16uint") {
    return DAWN_TEXTURE_FORMAT_R16_UINT;
  }
  if (webgpu_enum == "r16sint") {
    return DAWN_TEXTURE_FORMAT_R16_SINT;
  }
  if (webgpu_enum == "r16float") {
    return DAWN_TEXTURE_FORMAT_R16_FLOAT;
  }
  if (webgpu_enum == "rg8unorm") {
    return DAWN_TEXTURE_FORMAT_RG8_UNORM;
  }
  if (webgpu_enum == "rg8snorm") {
    return DAWN_TEXTURE_FORMAT_RG8_SNORM;
  }
  if (webgpu_enum == "rg8uint") {
    return DAWN_TEXTURE_FORMAT_RG8_UINT;
  }
  if (webgpu_enum == "rg8sint") {
    return DAWN_TEXTURE_FORMAT_RG8_SINT;
  }

  // Normal 32 bit formats
  if (webgpu_enum == "r32uint") {
    return DAWN_TEXTURE_FORMAT_R32_UINT;
  }
  if (webgpu_enum == "r32sint") {
    return DAWN_TEXTURE_FORMAT_R32_SINT;
  }
  if (webgpu_enum == "r32float") {
    return DAWN_TEXTURE_FORMAT_R32_FLOAT;
  }
  if (webgpu_enum == "rg16unorm") {
    return DAWN_TEXTURE_FORMAT_RG16_UNORM;
  }
  if (webgpu_enum == "rg16snorm") {
    return DAWN_TEXTURE_FORMAT_RG16_SNORM;
  }
  if (webgpu_enum == "rg16uint") {
    return DAWN_TEXTURE_FORMAT_RG16_UINT;
  }
  if (webgpu_enum == "rg16sint") {
    return DAWN_TEXTURE_FORMAT_RG16_SINT;
  }
  if (webgpu_enum == "rg16float") {
    return DAWN_TEXTURE_FORMAT_RG16_FLOAT;
  }
  if (webgpu_enum == "rgba8unorm") {
    return DAWN_TEXTURE_FORMAT_RGBA8_UNORM;
  }
  if (webgpu_enum == "rgba8unorm-srgb") {
    return DAWN_TEXTURE_FORMAT_RGBA8_UNORM_SRGB;
  }
  if (webgpu_enum == "rgba8snorm") {
    return DAWN_TEXTURE_FORMAT_RGBA8_SNORM;
  }
  if (webgpu_enum == "rgba8uint") {
    return DAWN_TEXTURE_FORMAT_RGBA8_UINT;
  }
  if (webgpu_enum == "rgba8sint") {
    return DAWN_TEXTURE_FORMAT_RGBA8_SINT;
  }
  if (webgpu_enum == "bgra8unorm") {
    return DAWN_TEXTURE_FORMAT_BGRA8_UNORM;
  }
  if (webgpu_enum == "bgra8unorm-srgb") {
    return DAWN_TEXTURE_FORMAT_BGRA8_UNORM_SRGB;
  }

  // Packed 32 bit formats
  if (webgpu_enum == "rgb10a2unorm") {
    return DAWN_TEXTURE_FORMAT_RGB10_A2_UNORM;
  }
  if (webgpu_enum == "rg11b10float") {
    return DAWN_TEXTURE_FORMAT_RG11_B10_FLOAT;
  }

  // Normal 64 bit formats
  if (webgpu_enum == "rg32uint") {
    return DAWN_TEXTURE_FORMAT_RG32_UINT;
  }
  if (webgpu_enum == "rg32sint") {
    return DAWN_TEXTURE_FORMAT_RG32_SINT;
  }
  if (webgpu_enum == "rg32float") {
    return DAWN_TEXTURE_FORMAT_RG32_FLOAT;
  }
  if (webgpu_enum == "rgba16unorm") {
    return DAWN_TEXTURE_FORMAT_RGBA16_UNORM;
  }
  if (webgpu_enum == "rgba16snorm") {
    return DAWN_TEXTURE_FORMAT_RGBA16_SNORM;
  }
  if (webgpu_enum == "rgba16uint") {
    return DAWN_TEXTURE_FORMAT_RGBA16_UINT;
  }
  if (webgpu_enum == "rgba16sint") {
    return DAWN_TEXTURE_FORMAT_RGBA16_SINT;
  }
  if (webgpu_enum == "rgba16float") {
    return DAWN_TEXTURE_FORMAT_RGBA16_FLOAT;
  }

  // Normal 128 bit formats
  if (webgpu_enum == "rgba32uint") {
    return DAWN_TEXTURE_FORMAT_RGBA32_UINT;
  }
  if (webgpu_enum == "rgba32sint") {
    return DAWN_TEXTURE_FORMAT_RGBA32_SINT;
  }
  if (webgpu_enum == "rgba32float") {
    return DAWN_TEXTURE_FORMAT_RGBA32_FLOAT;
  }

  // Depth / Stencil formats
  if (webgpu_enum == "depth32float") {
    return DAWN_TEXTURE_FORMAT_DEPTH32_FLOAT;
  }
  if (webgpu_enum == "depth24plus") {
    return DAWN_TEXTURE_FORMAT_DEPTH24_PLUS;
  }
  if (webgpu_enum == "depth24plus-stencil8") {
    return DAWN_TEXTURE_FORMAT_DEPTH24_PLUS_STENCIL8;
  }

  return DAWN_TEXTURE_FORMAT_FORCE32;
}

template <>
DawnTextureDimension AsDawnEnum<DawnTextureDimension>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "2d") {
    return DAWN_TEXTURE_DIMENSION_2D;
  }
  // TODO(crbug.com/dawn/129): Implement "1d" and "3d".
  NOTREACHED();
  return DAWN_TEXTURE_DIMENSION_FORCE32;
}

template <>
DawnTextureViewDimension AsDawnEnum<DawnTextureViewDimension>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "2d") {
    return DAWN_TEXTURE_VIEW_DIMENSION_2D;
  }
  if (webgpu_enum == "2d-array") {
    return DAWN_TEXTURE_VIEW_DIMENSION_2D_ARRAY;
  }
  if (webgpu_enum == "cube") {
    return DAWN_TEXTURE_VIEW_DIMENSION_CUBE;
  }
  if (webgpu_enum == "cube-array") {
    return DAWN_TEXTURE_VIEW_DIMENSION_CUBE_ARRAY;
  }
  // TODO(crbug.com/dawn/129): Implement "1d" and "3d".
  NOTREACHED();
  return DAWN_TEXTURE_VIEW_DIMENSION_FORCE32;
}

template <>
DawnStencilOperation AsDawnEnum<DawnStencilOperation>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "keep") {
    return DAWN_STENCIL_OPERATION_KEEP;
  }
  if (webgpu_enum == "zero") {
    return DAWN_STENCIL_OPERATION_ZERO;
  }
  if (webgpu_enum == "replace") {
    return DAWN_STENCIL_OPERATION_REPLACE;
  }
  if (webgpu_enum == "invert") {
    return DAWN_STENCIL_OPERATION_INVERT;
  }
  if (webgpu_enum == "increment-clamp") {
    return DAWN_STENCIL_OPERATION_INCREMENT_CLAMP;
  }
  if (webgpu_enum == "decrement-clamp") {
    return DAWN_STENCIL_OPERATION_DECREMENT_CLAMP;
  }
  if (webgpu_enum == "increment-wrap") {
    return DAWN_STENCIL_OPERATION_INCREMENT_WRAP;
  }
  if (webgpu_enum == "decrement-wrap") {
    return DAWN_STENCIL_OPERATION_DECREMENT_WRAP;
  }
  NOTREACHED();
  return DAWN_STENCIL_OPERATION_FORCE32;
}

template <>
DawnStoreOp AsDawnEnum<DawnStoreOp>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "store") {
    return DAWN_STORE_OP_STORE;
  }
  NOTREACHED();
  return DAWN_STORE_OP_FORCE32;
}

template <>
DawnLoadOp AsDawnEnum<DawnLoadOp>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "load") {
    return DAWN_LOAD_OP_LOAD;
  }
  NOTREACHED();
  return DAWN_LOAD_OP_FORCE32;
}

template <>
DawnIndexFormat AsDawnEnum<DawnIndexFormat>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "uint16") {
    return DAWN_INDEX_FORMAT_UINT16;
  }
  if (webgpu_enum == "uint32") {
    return DAWN_INDEX_FORMAT_UINT32;
  }
  NOTREACHED();
  return DAWN_INDEX_FORMAT_FORCE32;
}

template <>
DawnPrimitiveTopology AsDawnEnum<DawnPrimitiveTopology>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "point-list") {
    return DAWN_PRIMITIVE_TOPOLOGY_POINT_LIST;
  }
  if (webgpu_enum == "line-list") {
    return DAWN_PRIMITIVE_TOPOLOGY_LINE_LIST;
  }
  if (webgpu_enum == "line-strip") {
    return DAWN_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  }
  if (webgpu_enum == "triangle-list") {
    return DAWN_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }
  if (webgpu_enum == "triangle-strip") {
    return DAWN_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  }
  NOTREACHED();
  return DAWN_PRIMITIVE_TOPOLOGY_FORCE32;
}

template <>
DawnBlendFactor AsDawnEnum<DawnBlendFactor>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "zero") {
    return DAWN_BLEND_FACTOR_ZERO;
  }
  if (webgpu_enum == "one") {
    return DAWN_BLEND_FACTOR_ONE;
  }
  if (webgpu_enum == "src-color") {
    return DAWN_BLEND_FACTOR_SRC_COLOR;
  }
  if (webgpu_enum == "one-minus-src-color") {
    return DAWN_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
  }
  if (webgpu_enum == "src-alpha") {
    return DAWN_BLEND_FACTOR_SRC_ALPHA;
  }
  if (webgpu_enum == "one-minus-src-alpha") {
    return DAWN_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  }
  if (webgpu_enum == "dst-color") {
    return DAWN_BLEND_FACTOR_DST_COLOR;
  }
  if (webgpu_enum == "one-minus-dst-color") {
    return DAWN_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  }
  if (webgpu_enum == "dst-alpha") {
    return DAWN_BLEND_FACTOR_DST_ALPHA;
  }
  if (webgpu_enum == "one-minus-dst-alpha") {
    return DAWN_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  }
  if (webgpu_enum == "src-alpha-saturated") {
    return DAWN_BLEND_FACTOR_SRC_ALPHA_SATURATED;
  }
  if (webgpu_enum == "blend-color") {
    return DAWN_BLEND_FACTOR_BLEND_COLOR;
  }
  if (webgpu_enum == "one-minus-blend-color") {
    return DAWN_BLEND_FACTOR_ONE_MINUS_BLEND_COLOR;
  }
  NOTREACHED();
  return DAWN_BLEND_FACTOR_FORCE32;
}

template <>
DawnBlendOperation AsDawnEnum<DawnBlendOperation>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "add") {
    return DAWN_BLEND_OPERATION_ADD;
  }
  if (webgpu_enum == "subtract") {
    return DAWN_BLEND_OPERATION_SUBTRACT;
  }
  if (webgpu_enum == "reverse-subtract") {
    return DAWN_BLEND_OPERATION_REVERSE_SUBTRACT;
  }
  if (webgpu_enum == "min") {
    return DAWN_BLEND_OPERATION_MIN;
  }
  if (webgpu_enum == "max") {
    return DAWN_BLEND_OPERATION_MAX;
  }
  NOTREACHED();
  return DAWN_BLEND_OPERATION_FORCE32;
}

template <>
DawnInputStepMode AsDawnEnum<DawnInputStepMode>(
    const WTF::String& webgpu_enum) {
  if (webgpu_enum == "vertex") {
    return DAWN_INPUT_STEP_MODE_VERTEX;
  }
  if (webgpu_enum == "instance") {
    return DAWN_INPUT_STEP_MODE_INSTANCE;
  }
  NOTREACHED();
  return DAWN_INPUT_STEP_MODE_FORCE32;
}

template <>
DawnVertexFormat AsDawnEnum<DawnVertexFormat>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "uchar") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "uchar2") {
    return DAWN_VERTEX_FORMAT_UCHAR2;
  }
  if (webgpu_enum == "uchar3") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "uchar4") {
    return DAWN_VERTEX_FORMAT_UCHAR4;
  }
  if (webgpu_enum == "char") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "char2") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "char3") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "char4") {
    return DAWN_VERTEX_FORMAT_CHAR4;
  }
  if (webgpu_enum == "ucharnorm") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "uchar2norm") {
    return DAWN_VERTEX_FORMAT_UCHAR2_NORM;
  }
  if (webgpu_enum == "uchar3norm") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "uchar4norm") {
    return DAWN_VERTEX_FORMAT_UCHAR4_NORM;
  }
  if (webgpu_enum == "uchar4norm-bgra") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "charnorm") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "char2norm") {
    return DAWN_VERTEX_FORMAT_CHAR2_NORM;
  }
  if (webgpu_enum == "char3norm") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "char4norm") {
    return DAWN_VERTEX_FORMAT_CHAR4_NORM;
  }
  if (webgpu_enum == "ushort") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "ushort2") {
    return DAWN_VERTEX_FORMAT_USHORT2;
  }
  if (webgpu_enum == "ushort3") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "ushort4") {
    return DAWN_VERTEX_FORMAT_USHORT4;
  }
  if (webgpu_enum == "short") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "short2") {
    return DAWN_VERTEX_FORMAT_SHORT2;
  }
  if (webgpu_enum == "short3") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "short4") {
    return DAWN_VERTEX_FORMAT_SHORT4;
  }
  if (webgpu_enum == "ushortnorm") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "ushort2norm") {
    return DAWN_VERTEX_FORMAT_USHORT2_NORM;
  }
  if (webgpu_enum == "ushort3norm") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "ushort4norm") {
    return DAWN_VERTEX_FORMAT_USHORT4_NORM;
  }
  if (webgpu_enum == "shortnorm") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "short2norm") {
    return DAWN_VERTEX_FORMAT_SHORT2_NORM;
  }
  if (webgpu_enum == "short3norm") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "short4norm") {
    return DAWN_VERTEX_FORMAT_SHORT4_NORM;
  }
  if (webgpu_enum == "half") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "half2") {
    return DAWN_VERTEX_FORMAT_HALF2;
  }
  if (webgpu_enum == "half3") {
    // TODO(crbug.com/dawn/41): Implement remaining vertex formats
    NOTREACHED();
    return DAWN_VERTEX_FORMAT_FORCE32;
  }
  if (webgpu_enum == "half4") {
    return DAWN_VERTEX_FORMAT_HALF4;
  }
  if (webgpu_enum == "float") {
    return DAWN_VERTEX_FORMAT_FLOAT;
  }
  if (webgpu_enum == "float2") {
    return DAWN_VERTEX_FORMAT_FLOAT2;
  }
  if (webgpu_enum == "float3") {
    return DAWN_VERTEX_FORMAT_FLOAT3;
  }
  if (webgpu_enum == "float4") {
    return DAWN_VERTEX_FORMAT_FLOAT4;
  }
  if (webgpu_enum == "uint") {
    return DAWN_VERTEX_FORMAT_UINT;
  }
  if (webgpu_enum == "uint2") {
    return DAWN_VERTEX_FORMAT_UINT2;
  }
  if (webgpu_enum == "uint3") {
    return DAWN_VERTEX_FORMAT_UINT3;
  }
  if (webgpu_enum == "uint4") {
    return DAWN_VERTEX_FORMAT_UINT4;
  }
  if (webgpu_enum == "int") {
    return DAWN_VERTEX_FORMAT_INT;
  }
  if (webgpu_enum == "int2") {
    return DAWN_VERTEX_FORMAT_INT2;
  }
  if (webgpu_enum == "int3") {
    return DAWN_VERTEX_FORMAT_INT3;
  }
  if (webgpu_enum == "int4") {
    return DAWN_VERTEX_FORMAT_INT4;
  }
  NOTREACHED();
  return DAWN_VERTEX_FORMAT_FORCE32;
}

template <>
DawnAddressMode AsDawnEnum<DawnAddressMode>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "clamp-to-edge") {
    return DAWN_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
  if (webgpu_enum == "repeat") {
    return DAWN_ADDRESS_MODE_REPEAT;
  }
  if (webgpu_enum == "mirror-repeat") {
    return DAWN_ADDRESS_MODE_MIRROR_REPEAT;
  }
  NOTREACHED();
  return DAWN_ADDRESS_MODE_FORCE32;
}

template <>
DawnFilterMode AsDawnEnum<DawnFilterMode>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "nearest") {
    return DAWN_FILTER_MODE_NEAREST;
  }
  if (webgpu_enum == "linear") {
    return DAWN_FILTER_MODE_LINEAR;
  }
  NOTREACHED();
  return DAWN_FILTER_MODE_FORCE32;
}

template <>
DawnCullMode AsDawnEnum<DawnCullMode>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "none") {
    return DAWN_CULL_MODE_NONE;
  }
  if (webgpu_enum == "front") {
    return DAWN_CULL_MODE_FRONT;
  }
  if (webgpu_enum == "back") {
    return DAWN_CULL_MODE_BACK;
  }
  NOTREACHED();
  return DAWN_CULL_MODE_FORCE32;
}

template <>
DawnFrontFace AsDawnEnum<DawnFrontFace>(const WTF::String& webgpu_enum) {
  if (webgpu_enum == "ccw") {
    return DAWN_FRONT_FACE_CCW;
  }
  if (webgpu_enum == "cw") {
    return DAWN_FRONT_FACE_CW;
  }
  NOTREACHED();
  return DAWN_FRONT_FACE_FORCE32;
}

DawnColor AsDawnType(const GPUColor* webgpu_color) {
  DCHECK(webgpu_color);

  DawnColor dawn_color = {};
  dawn_color.r = webgpu_color->r();
  dawn_color.g = webgpu_color->g();
  dawn_color.b = webgpu_color->b();
  dawn_color.a = webgpu_color->a();

  return dawn_color;
}

DawnExtent3D AsDawnType(const GPUExtent3D* webgpu_extent) {
  DCHECK(webgpu_extent);

  DawnExtent3D dawn_extent = {};
  dawn_extent.width = webgpu_extent->width();
  dawn_extent.height = webgpu_extent->height();
  dawn_extent.depth = webgpu_extent->depth();

  return dawn_extent;
}

DawnOrigin3D AsDawnType(const GPUOrigin3D* webgpu_origin) {
  DCHECK(webgpu_origin);

  DawnOrigin3D dawn_origin = {};
  dawn_origin.x = webgpu_origin->x();
  dawn_origin.y = webgpu_origin->y();
  dawn_origin.z = webgpu_origin->z();

  return dawn_origin;
}

OwnedPipelineStageDescriptor AsDawnType(
    const GPUPipelineStageDescriptor* webgpu_stage) {
  DCHECK(webgpu_stage);

  std::string entry_point = webgpu_stage->entryPoint().Ascii();
  // length() is in bytes (not utf-8 characters or something), so this is ok.
  size_t byte_size = entry_point.length() + 1;

  std::unique_ptr<char[]> entry_point_keepalive =
      std::make_unique<char[]>(byte_size);
  char* entry_point_ptr = entry_point_keepalive.get();
  memcpy(entry_point_ptr, entry_point.c_str(), byte_size);

  DawnPipelineStageDescriptor dawn_stage = {};
  dawn_stage.module = webgpu_stage->module()->GetHandle();
  dawn_stage.entryPoint = entry_point_ptr;

  return std::make_tuple(dawn_stage, std::move(entry_point_keepalive));
}

}  // namespace blink
