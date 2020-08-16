// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metal_util/hdr_copier_layer.h"

#include <CoreVideo/CVPixelBuffer.h>
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "components/metal_util/device.h"
#include "ui/gfx/color_space.h"

namespace {

// Source of the shader to perform tonemapping.
// TODO(https://crbug.com/1101041): This shader just does a copy for now.
const char* tonemapping_shader_source =
    "#include <metal_stdlib>\n"
    "#include <simd/simd.h>\n"
    "using metal::float2;\n"
    "using metal::float3x3;\n"
    "using metal::float4;\n"
    "using metal::sampler;\n"
    "using metal::texture2d;\n"
    "\n"
    "typedef struct {\n"
    "    float4 clipSpacePosition [[position]];\n"
    "    float2 texcoord;\n"
    "} RasterizerData;\n"
    "\n"
    "vertex RasterizerData vertexShader(\n"
    "    uint vertexID [[vertex_id]],\n"
    "    constant float2 *positions[[buffer(0)]]) {\n"
    "  RasterizerData out;\n"
    "  out.clipSpacePosition = vector_float4(0.f, 0.f, 0.f, 1.f);\n"
    "  out.clipSpacePosition.x = 2.f * positions[vertexID].x - 1.f;\n"
    "  out.clipSpacePosition.y = -2.f * positions[vertexID].y + 1.f;\n"
    "  out.texcoord = positions[vertexID];\n"
    "  return out;\n"
    "}\n"
    "\n"
    "fragment float4 fragmentShader(RasterizerData in [[stage_in]],\n"
    "                               texture2d<float> t [[texture(0)]],\n"
    "                               constant float3x3& m [[buffer(0)]],\n"
    "                               constant uint32_t& f [[buffer(1)]]) {\n"
    "    constexpr sampler s(metal::mag_filter::nearest,\n"
    "                        metal::min_filter::nearest);\n"
    "    float4 color = t.sample(s, in.texcoord);\n"
    "    return color;\n"
    "}\n";

// Convert from an IOSurface's pixel format to a MTLPixelFormat. Crash on any
// unsupported formats.
MTLPixelFormat IOSurfaceGetMTLPixelFormat(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.13)) {
  uint32_t format = IOSurfaceGetPixelFormat(buffer);
  switch (format) {
    case kCVPixelFormatType_64RGBAHalf:
      return MTLPixelFormatRGBA16Float;
    case kCVPixelFormatType_ARGB2101010LEPacked:
      return MTLPixelFormatBGR10A2Unorm;
    default:
      break;
  }
  return MTLPixelFormatInvalid;
}

// Retrieve the named color space from an IOSurface and convert it to a
// CGColorSpace. Return nullptr on failure.
CGColorSpaceRef IOSurfaceCopyCGColorSpace(IOSurfaceRef buffer) {
  base::ScopedCFTypeRef<CFTypeRef> color_space_value(
      IOSurfaceCopyValue(buffer, CFSTR("IOSurfaceColorSpace")));
  if (!color_space_value)
    return nullptr;
  CFStringRef color_space_string =
      base::mac::CFCast<CFStringRef>(color_space_value);
  if (!color_space_string)
    return nullptr;
  base::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateWithName(color_space_string));
  if (!color_space)
    return nullptr;
  return color_space.release();
}

base::scoped_nsprotocol<id<MTLRenderPipelineState>> CreateRenderPipelineState(
    id<MTLDevice> device) API_AVAILABLE(macos(10.13)) {
  base::scoped_nsprotocol<id<MTLRenderPipelineState>> render_pipeline_state;

  base::scoped_nsprotocol<id<MTLLibrary>> library;
  {
    NSError* error = nil;
    base::scoped_nsobject<NSString> source([[NSString alloc]
        initWithCString:tonemapping_shader_source
               encoding:NSASCIIStringEncoding]);
    base::scoped_nsobject<MTLCompileOptions> options(
        [[MTLCompileOptions alloc] init]);
    library.reset([device newLibraryWithSource:source
                                       options:options
                                         error:&error]);
    if (error) {
      NSLog(@"Failed to compile shader: %@", error);
      return render_pipeline_state;
    }
  }

  {
    base::scoped_nsprotocol<id<MTLFunction>> vertex_function(
        [library newFunctionWithName:@"vertexShader"]);
    base::scoped_nsprotocol<id<MTLFunction>> fragment_function(
        [library newFunctionWithName:@"fragmentShader"]);
    NSError* error = nil;
    base::scoped_nsobject<MTLRenderPipelineDescriptor> desc(
        [[MTLRenderPipelineDescriptor alloc] init]);
    [desc setVertexFunction:vertex_function];
    [desc setFragmentFunction:fragment_function];
    [[desc colorAttachments][0] setPixelFormat:MTLPixelFormatRGBA16Float];
    render_pipeline_state.reset(
        [device newRenderPipelineStateWithDescriptor:desc error:&error]);
    if (error) {
      NSLog(@"Failed to create render pipeline state: %@", error);
      return render_pipeline_state;
    }
  }

  return render_pipeline_state;
}

}  // namespace

#if !defined(MAC_OS_X_VERSION_10_15)
API_AVAILABLE(macos(10.15))
@interface CAMetalLayer (Forward)
@property(readonly) id<MTLDevice> preferredDevice;
@end
#endif

API_AVAILABLE(macos(10.15))
@interface HDRCopierLayer : CAMetalLayer {
  base::scoped_nsprotocol<id<MTLRenderPipelineState>> _render_pipeline_state;
}
- (id)init;
- (void)setHDRContents:(IOSurfaceRef)buffer
        withColorSpace:(gfx::ColorSpace)color_space;
@end

@implementation HDRCopierLayer
- (id)init {
  if (self = [super init]) {
    base::scoped_nsprotocol<id<MTLDevice>> device(metal::CreateDefaultDevice());
    [self setWantsExtendedDynamicRangeContent:YES];
    [self setDevice:device];
    [self setOpaque:NO];
    [self setPresentsWithTransaction:YES];
  }
  return self;
}

- (void)setHDRContents:(IOSurfaceRef)buffer
        withColorSpace:(gfx::ColorSpace)color_space {
  // Retrieve information about the IOSurface.
  size_t width = IOSurfaceGetWidth(buffer);
  size_t height = IOSurfaceGetHeight(buffer);
  MTLPixelFormat mtl_format = IOSurfaceGetMTLPixelFormat(buffer);
  if (mtl_format == MTLPixelFormatInvalid) {
    DLOG(ERROR) << "Unsupported IOSurface format.";
    return;
  }
  base::ScopedCFTypeRef<CGColorSpaceRef> cg_color_space(
      IOSurfaceCopyCGColorSpace(buffer));
  if (!cg_color_space) {
    DLOG(ERROR) << "Unsupported IOSurface color space.";
  }

  // Migrate to the MTLDevice on which the CAMetalLayer is being composited, if
  // known.
  if ([self respondsToSelector:@selector(preferredDevice)]) {
    id<MTLDevice> preferred_device = nil;
    if (preferred_device)
      [self setDevice:preferred_device];
  }
  id<MTLDevice> device = [self device];

  // When the device changes, rebuild the RenderPipelineState.
  if (device != [_render_pipeline_state device])
    _render_pipeline_state = CreateRenderPipelineState(device);
  if (!_render_pipeline_state)
    return;

  // Update the layer's properties to match the IOSurface.
  [self setDrawableSize:CGSizeMake(width, height)];
  [self setPixelFormat:MTLPixelFormatRGBA16Float];
  [self setColorspace:cg_color_space];

  // Create a texture to wrap the IOSurface.
  base::scoped_nsprotocol<id<MTLTexture>> buffer_texture;
  {
    base::scoped_nsobject<MTLTextureDescriptor> tex_desc(
        [MTLTextureDescriptor new]);
    [tex_desc setTextureType:MTLTextureType2D];
    [tex_desc setUsage:MTLTextureUsageShaderRead];
    [tex_desc setPixelFormat:mtl_format];
    [tex_desc setWidth:width];
    [tex_desc setHeight:height];
    [tex_desc setDepth:1];
    [tex_desc setMipmapLevelCount:1];
    [tex_desc setArrayLength:1];
    [tex_desc setSampleCount:1];
    [tex_desc setStorageMode:MTLStorageModeManaged];
    buffer_texture.reset([device newTextureWithDescriptor:tex_desc
                                                iosurface:buffer
                                                    plane:0]);
  }

  // Create a texture to wrap the drawable.
  id<CAMetalDrawable> drawable = [self nextDrawable];
  id<MTLTexture> drawable_texture = [drawable texture];

  // Copy from the IOSurface to the drawable.
  base::scoped_nsprotocol<id<MTLCommandQueue>> command_queue(
      [device newCommandQueue]);
  id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
  id<MTLRenderCommandEncoder> encoder = nil;
  {
    MTLRenderPassDescriptor* desc =
        [MTLRenderPassDescriptor renderPassDescriptor];
    desc.colorAttachments[0].texture = drawable_texture;
    desc.colorAttachments[0].loadAction = MTLLoadActionClear;
    desc.colorAttachments[0].storeAction = MTLStoreActionStore;
    desc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
    encoder = [command_buffer renderCommandEncoderWithDescriptor:desc];

    MTLViewport viewport;
    viewport.originX = 0;
    viewport.originY = 0;
    viewport.width = width;
    viewport.height = height;
    viewport.znear = -1.0;
    viewport.zfar = 1.0;
    [encoder setViewport:viewport];
    [encoder setRenderPipelineState:_render_pipeline_state];
    [encoder setFragmentTexture:buffer_texture atIndex:0];
  }
  {
    simd::float2 positions[6] = {
        simd::make_float2(0, 0), simd::make_float2(0, 1),
        simd::make_float2(1, 1), simd::make_float2(1, 1),
        simd::make_float2(1, 0), simd::make_float2(0, 0),
    };
    simd::float3x3 matrix(simd::make_float3(1, 0, 0),
                          simd::make_float3(0, 1, 0),
                          simd::make_float3(0, 0, 1));
    [encoder setVertexBytes:positions length:sizeof(positions) atIndex:0];
    [encoder setFragmentBytes:&matrix length:sizeof(matrix) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                vertexStart:0
                vertexCount:6];
  }
  [encoder endEncoding];
  [command_buffer commit];
  [command_buffer waitUntilScheduled];
  [drawable present];
}
@end

namespace metal {

CALayer* CreateHDRCopierLayer() {
  // If this is hit by non-10.15 paths (e.g, for testing), then return an
  // ordinary CALayer. Calling setContents on that CALayer will work fine
  // (HDR content will be clipped, but that would have happened anyway).
  if (@available(macos 10.15, *))
    return [[HDRCopierLayer alloc] init];
  NOTREACHED();
  return nil;
}

void UpdateHDRCopierLayer(CALayer* layer,
                          IOSurfaceRef buffer,
                          const gfx::ColorSpace& color_space) {
  if (@available(macos 10.15, *)) {
    if (auto* hdr_copier_layer = base::mac::ObjCCast<HDRCopierLayer>(layer)) {
      [hdr_copier_layer setHDRContents:buffer withColorSpace:color_space];
      return;
    }
  }
  NOTREACHED();
}

bool ShouldUseHDRCopier(IOSurfaceRef buffer,
                        const gfx::ColorSpace& color_space) {
  if (@available(macos 10.15, *)) {
    return color_space.IsHDR() &&
           IOSurfaceGetMTLPixelFormat(buffer) != MTLPixelFormatInvalid;
  }
  return false;
}

}  // namespace metal
