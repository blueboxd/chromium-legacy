// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_video.h"

#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class LayoutVideoTest : public RenderingTest {
 public:
  void CreateAndSetImage(const char* id, int width, int height) {
    // Create one image with size(width, height)
    sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
    SkImageInfo raster_image_info =
        SkImageInfo::MakeN32Premul(width, height, src_rgb_color_space);
    sk_sp<SkSurface> surface(SkSurface::MakeRaster(raster_image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    ImageResourceContent* image_content = ImageResourceContent::CreateLoaded(
        UnacceleratedStaticBitmapImage::Create(image).get());

    // Set image to video
    auto* layout_image = (LayoutImage*)GetLayoutObjectByElementId(id);
    layout_image->ImageResource()->SetImageResource(image_content);
  }
};

TEST_F(LayoutVideoTest, PosterSizeWithNormal) {
  SetBodyInnerHTML(R"HTML(
    <style>
      video {zoom:1}
    </style>
    <video id='video' />
  )HTML");

  CreateAndSetImage("video", 10, 10);
  UpdateAllLifecyclePhasesForTest();

  int width = ((LayoutBox*)GetLayoutObjectByElementId("video"))
                  ->AbsoluteBoundingBoxRect()
                  .Width();
  EXPECT_EQ(width, 10);
}

TEST_F(LayoutVideoTest, PosterSizeWithZoom) {
  SetBodyInnerHTML(R"HTML(
    <style>
      video {zoom:1.5}
    </style>
    <video id='video' />
  )HTML");

  CreateAndSetImage("video", 10, 10);
  UpdateAllLifecyclePhasesForTest();

  int width = ((LayoutBox*)GetLayoutObjectByElementId("video"))
                  ->AbsoluteBoundingBoxRect()
                  .Width();
  EXPECT_EQ(width, 15);
}

}  // namespace blink
