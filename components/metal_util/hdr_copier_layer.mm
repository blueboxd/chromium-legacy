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
#include "components/metal_util/device.h"

#include "base/strings/sys_string_conversions.h"

namespace {

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
  NOTREACHED();
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

}  // namespace

#if !defined(MAC_OS_X_VERSION_10_15)
API_AVAILABLE(macos(10.15))
@interface CAMetalLayer (Forward)
@property(readonly) id<MTLDevice> preferredDevice;
@end
#endif

API_AVAILABLE(macos(10.15))
@interface HDRCopierLayer : CAMetalLayer
- (id)init;
- (void)setContents:(id)contents;
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

- (void)setContents:(id)contents {
  IOSurfaceRef buffer = reinterpret_cast<IOSurfaceRef>(contents);

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

  // Update the layer's properties to match the IOSurface.
  [self setDrawableSize:CGSizeMake(width, height)];
  [self setPixelFormat:mtl_format];
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
  id<MTLBlitCommandEncoder> encoder = [command_buffer blitCommandEncoder];
  [encoder copyFromTexture:buffer_texture
               sourceSlice:0
               sourceLevel:0
              sourceOrigin:MTLOriginMake(0, 0, 0)
                sourceSize:MTLSizeMake(width, height, 1)
                 toTexture:drawable_texture
          destinationSlice:0
          destinationLevel:0
         destinationOrigin:MTLOriginMake(0, 0, 0)];
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
  else
    return [[CALayer alloc] init];
}

}  // namespace metal
