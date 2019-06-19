// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_element_timing.h"

#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

namespace internal {

extern bool IsExplicitlyRegisteredForTiming(const LayoutObject* layout_object);

}

class ImageElementTimingTest : public RenderingTest {
 protected:
  // Sets an image resource for the LayoutImage with the given |id| and return
  // the LayoutImage.
  LayoutImage* SetImageResource(const char* id, int width, int height) {
    ImageResourceContent* content = CreateImageForTest(width, height);
    auto* layout_image = ToLayoutImage(GetLayoutObjectByElementId(id));
    layout_image->ImageResource()->SetImageResource(content);
    return layout_image;
  }

  // Similar to above but for a LayoutSVGImage.
  LayoutSVGImage* SetSVGImageResource(const char* id, int width, int height) {
    ImageResourceContent* content = CreateImageForTest(width, height);
    auto* layout_image = ToLayoutSVGImage(GetLayoutObjectByElementId(id));
    layout_image->ImageResource()->SetImageResource(content);
    return layout_image;
  }

  const WTF::HashSet<const LayoutObject*>& GetImagesNotified() {
    return ImageElementTiming::From(*GetDocument().domWindow())
        .images_notified_;
  }

  const WTF::HashSet<
      std::pair<const LayoutObject*, const ImageResourceContent*>>&
  GetBackgroundImagesNotified() {
    return ImageElementTiming::From(*GetDocument().domWindow())
        .background_images_notified_;
  }

 private:
  ImageResourceContent* CreateImageForTest(int width, int height) {
    sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
    SkImageInfo raster_image_info =
        SkImageInfo::MakeN32Premul(width, height, src_rgb_color_space);
    sk_sp<SkSurface> surface(SkSurface::MakeRaster(raster_image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    ImageResourceContent* original_image_resource =
        ImageResourceContent::CreateLoaded(
            StaticBitmapImage::Create(image).get());
    return original_image_resource;
  }
};

TEST_F(ImageElementTimingTest, TestIsExplicitlyRegisteredForTiming) {
  EnableCompositing();
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <img id="missing-attribute" style='width: 100px; height: 100px;'/>
    <img id="unset-attribute" elementtiming style='width: 100px; height: 100px;'/>
    <img id="empty-attribute" elementtiming="" style='width: 100px; height: 100px;'/>
    <img id="valid-attribute" elementtiming="valid-id" style='width: 100px; height: 100px;'/>
  )HTML");

  LayoutObject* without_attribute =
      GetLayoutObjectByElementId("missing-attribute");
  bool actual = internal::IsExplicitlyRegisteredForTiming(without_attribute);
  EXPECT_FALSE(actual) << "Nodes without an 'elementtiming' attribute should "
                          "not be explicitly registered.";

  LayoutObject* with_undefined_attribute =
      GetLayoutObjectByElementId("unset-attribute");
  actual = internal::IsExplicitlyRegisteredForTiming(with_undefined_attribute);
  EXPECT_FALSE(actual) << "Nodes with undefined 'elementtiming' attribute "
                          "should not be explicitly registered.";

  LayoutObject* with_empty_attribute =
      GetLayoutObjectByElementId("empty-attribute");
  actual = internal::IsExplicitlyRegisteredForTiming(with_empty_attribute);
  EXPECT_FALSE(actual) << "Nodes with an empty 'elementtiming' attribute "
                          "should not be explicitly registered.";

  LayoutObject* with_explicit_element_timing =
      GetLayoutObjectByElementId("valid-attribute");
  actual =
      internal::IsExplicitlyRegisteredForTiming(with_explicit_element_timing);
  EXPECT_TRUE(actual) << "Nodes with a non-empty 'elementtiming' attribute "
                         "should be explicitly registered.";
}

TEST_F(ImageElementTimingTest, IgnoresUnmarkedElement) {
  // Tests that, if the 'elementtiming' attribute is missing, the element isn't
  // considered by ImageElementTiming.
  EnableCompositing();
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <img id="target" style='width: 100px; height: 100px;'/>
  )HTML");
  LayoutImage* layout_image = SetImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetImagesNotified().Contains(layout_image));
}

TEST_F(ImageElementTimingTest, ImageInsideSVG) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <svg mask="url(#mask)">
      <mask id="mask">
        <foreignObject width="100" height="100">
          <img elementtiming="image-inside-svg" id="target" style='width: 100px; height: 100px;'/>
        </foreignObject>
      </mask>
      <rect width="100" height="100" fill="green"/>
    </svg>
  )HTML");
  LayoutImage* layout_image = SetImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  // Enable compositing and also update document lifecycle.
  EnableCompositing();

  // |layout_image| should have had its paint notified to ImageElementTiming.
  EXPECT_TRUE(GetImagesNotified().Contains(layout_image));
}

TEST_F(ImageElementTimingTest, ImageRemoved) {
  EnableCompositing();
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <img elementtiming="will-be-removed" id="target" style='width: 100px; height: 100px;'/>
  )HTML");
  LayoutImage* layout_image = SetImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetImagesNotified().Contains(layout_image));

  GetDocument().getElementById("target")->remove();
  // |layout_image| should no longer be part of |images_notified| since it will
  // be destroyed.
  EXPECT_TRUE(GetImagesNotified().IsEmpty());
}

TEST_F(ImageElementTimingTest, SVGImageRemoved) {
  EnableCompositing();
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <svg>
      <image elementtiming="svg-will-be-removed" id="target" style='width: 100px; height: 100px;'/>
    </svg>
  )HTML");
  LayoutSVGImage* layout_image = SetSVGImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetImagesNotified().Contains(layout_image));

  GetDocument().getElementById("target")->remove();
  // |layout_image| should no longer be part of |images_notified| since it will
  // be destroyed.
  EXPECT_TRUE(GetImagesNotified().IsEmpty());
}

TEST_F(ImageElementTimingTest, BackgroundImageRemoved) {
  EnableCompositing();
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        background: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
    </style>
    <div elementtiming="time-my-background-image" id="target">/div>
  )HTML");
  LayoutObject* object = GetLayoutObjectByElementId("target");
  ImageResourceContent* content =
      object->Style()->BackgroundLayers().GetImage()->CachedImage();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(GetBackgroundImagesNotified().size(), 1u);
  EXPECT_TRUE(
      GetBackgroundImagesNotified().Contains(std::make_pair(object, content)));

  GetDocument().getElementById("target")->remove();
  EXPECT_TRUE(GetBackgroundImagesNotified().IsEmpty());
}

}  // namespace blink
