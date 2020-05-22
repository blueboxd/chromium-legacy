// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/delegated_ink/delegated_ink_trail_presenter.h"

#include "components/viz/common/delegated_ink_metadata.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/modules/delegated_ink/ink_trail_style.h"

namespace blink {
namespace {

class TestDelegatedInkMetadata {
 public:
  explicit TestDelegatedInkMetadata(viz::DelegatedInkMetadata* metadata)
      : point_(metadata->point()),
        color_(metadata->color()),
        diameter_(metadata->diameter()),
        area_(metadata->presentation_area()) {}
  explicit TestDelegatedInkMetadata(gfx::RectF area,
                                    float device_pixel_ratio = 1.0)
      : area_(area) {
    area_.Scale(device_pixel_ratio);
  }

  void ExpectEqual(TestDelegatedInkMetadata actual) const {
    // LayoutUnits cast floats to ints, causing the actual point and area to be
    // off a small amount from what is expected.
    EXPECT_NEAR(point_.x(), actual.point_.x(), LayoutUnit::Epsilon());
    EXPECT_NEAR(point_.y(), actual.point_.y(), LayoutUnit::Epsilon());
    EXPECT_EQ(color_, actual.color_);
    EXPECT_EQ(diameter_, actual.diameter_);
    EXPECT_NEAR(area_.x(), actual.area_.x(), LayoutUnit::Epsilon());
    EXPECT_NEAR(area_.y(), actual.area_.y(), LayoutUnit::Epsilon());
    EXPECT_NEAR(area_.width(), actual.area_.width(), LayoutUnit::Epsilon());
    EXPECT_NEAR(area_.height(), actual.area_.height(), LayoutUnit::Epsilon());
  }

  void SetPoint(gfx::PointF pt) { point_ = pt; }
  void SetColor(SkColor color) { color_ = color; }
  void SetDiameter(double diameter) { diameter_ = diameter; }
  void SetArea(gfx::RectF area) { area_ = area; }

 private:
  gfx::PointF point_;
  SkColor color_;
  double diameter_;
  gfx::RectF area_;
};

DelegatedInkTrailPresenter* CreatePresenter(Element* element,
                                            LocalFrame* frame) {
  return DelegatedInkTrailPresenter::CreatePresenter(element, frame);
}

}  // namespace

class DelegatedInkTrailPresenterUnitTest : public SimTest {
 protected:
  PointerEvent* CreatePointerMoveEvent(gfx::PointF pt) {
    PointerEventInit* init = PointerEventInit::Create();
    init->setClientX(pt.x());
    init->setClientY(pt.y());
    PointerEvent* event = PointerEvent::Create("pointermove", init);
    event->SetTrusted(true);
    return event;
  }

  TestDelegatedInkMetadata GetActualMetadata() {
    return TestDelegatedInkMetadata(
        WebWidgetClient().layer_tree_host()->DelegatedInkMetadataForTesting());
  }

  void SetPageZoomFactor(const float zoom) {
    GetDocument().GetFrame()->SetPageZoomFactor(zoom);
  }
};

// Confirm that all the information is collected and transformed correctly, if
// necessary. Numbers and color used were chosen arbitrarily.
TEST_F(DelegatedInkTrailPresenterUnitTest, CollectAndPropagateMetadata) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 191px;
      height: 234px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  const float kCanvasWidth = 191.f;
  const float kCanvasHeight = 234.f;

  TestDelegatedInkMetadata expected_metadata(
      gfx::RectF(0, 0, kCanvasWidth, kCanvasHeight));

  DelegatedInkTrailPresenter* presenter = CreatePresenter(
      GetDocument().getElementById("canvas"), GetDocument().GetFrame());
  DCHECK(presenter);

  InkTrailStyle style;
  style.setDiameter(5);
  style.setColor("blue");
  expected_metadata.SetDiameter(style.diameter());
  expected_metadata.SetColor(SK_ColorBLUE);

  gfx::PointF pt(100, 100);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(GetDocument().GetFrame()),
      CreatePointerMoveEvent(pt), &style);
  expected_metadata.SetPoint(pt);

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that presentation area defaults to the size of the viewport.
// Numbers and color used were chosen arbitrarily.
TEST_F(DelegatedInkTrailPresenterUnitTest, PresentationAreaNotProvided) {
  const int kViewportHeight = 555;
  const int kViewportWidth = 333;
  WebView().MainFrameWidget()->Resize(WebSize(kViewportWidth, kViewportHeight));

  DelegatedInkTrailPresenter* presenter =
      CreatePresenter(nullptr, GetDocument().GetFrame());
  DCHECK(presenter);

  TestDelegatedInkMetadata expected_metadata(
      gfx::RectF(0, 0, kViewportWidth, kViewportHeight));

  InkTrailStyle style;
  style.setDiameter(3.6);
  style.setColor("yellow");
  expected_metadata.SetDiameter(style.diameter());
  expected_metadata.SetColor(SK_ColorYELLOW);

  gfx::PointF pt(70, 109);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(GetDocument().GetFrame()),
      CreatePointerMoveEvent(pt), &style);
  expected_metadata.SetPoint(pt);

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that everything is still calculated correctly when the
// DevicePixelRatio is not 1. Numbers and color used were chosen arbitrarily.
TEST_F(DelegatedInkTrailPresenterUnitTest, NotDefaultDevicePixelRatio) {
  const float kZoom = 1.7;
  SetPageZoomFactor(kZoom);

  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 281px;
      height: 190px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  const float kCanvasWidth = 281.f;
  const float kCanvasHeight = 190.f;

  TestDelegatedInkMetadata expected_metadata(
      gfx::RectF(0, 0, kCanvasWidth, kCanvasHeight), kZoom);

  DelegatedInkTrailPresenter* presenter = CreatePresenter(
      GetDocument().getElementById("canvas"), GetDocument().GetFrame());
  DCHECK(presenter);

  InkTrailStyle style;
  style.setDiameter(101.5);
  style.setColor("magenta");
  expected_metadata.SetDiameter(style.diameter() * kZoom);
  expected_metadata.SetColor(SK_ColorMAGENTA);

  gfx::PointF pt(87, 113);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(GetDocument().GetFrame()),
      CreatePointerMoveEvent(pt), &style);
  pt.Scale(kZoom);
  expected_metadata.SetPoint(pt);

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that the offset is correct. Numbers and color used were chosen
// arbitrarily.
TEST_F(DelegatedInkTrailPresenterUnitTest, CanvasNotAtOrigin) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 250px;
      height: 350px;
      position: fixed;
      top: 375px;
      left: 166px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  const float kCanvasWidth = 250.f;
  const float kCanvasHeight = 350.f;
  const float kCanvasTopOffset = 375.f;
  const float kCanvasLeftOffset = 166.f;

  TestDelegatedInkMetadata expected_metadata(gfx::RectF(
      kCanvasLeftOffset, kCanvasTopOffset, kCanvasWidth, kCanvasHeight));

  DelegatedInkTrailPresenter* presenter = CreatePresenter(
      GetDocument().getElementById("canvas"), GetDocument().GetFrame());
  DCHECK(presenter);

  InkTrailStyle style;
  style.setDiameter(8.6);
  style.setColor("red");
  expected_metadata.SetDiameter(style.diameter());
  expected_metadata.SetColor(SK_ColorRED);

  gfx::PointF pt(380, 175);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(GetDocument().GetFrame()),
      CreatePointerMoveEvent(pt), &style);
  expected_metadata.SetPoint(pt);

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that values, specifically offsets, are transformed correctly when
// the canvas is in an iframe. Numbers and color used were chosen arbitrarily.
TEST_F(DelegatedInkTrailPresenterUnitTest, CanvasInIFrame) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    iframe {
      width: 500px;
      height: 500px;
      position: fixed;
      top: 26px;
      left: 57px;
    }
    </style>
    <iframe id='iframe' src='https://example.com/iframe.html'>
    </iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 250px;
      height: 250px;
      position: fixed;
      top: 33px;
      left: 16px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  // When creating the expected metadata, we have to take into account the
  // offsets that are applied to the iframe that the canvas is in, and the 2px
  // border around the iframe.
  const float kIframeBorder = 2.f;
  const float kIframeLeftOffset = 57.f + kIframeBorder;
  const float kIframeTopOffset = 26.f + kIframeBorder;
  const float kCanvasLeftOffset = 16.f;
  const float kCanvasTopOffset = 33.f;
  const float kCanvasHeight = 250.f;
  const float kCanvasWidth = 250.f;

  auto* iframe_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("iframe"));
  auto* iframe_localframe = To<LocalFrame>(iframe_element->ContentFrame());
  Document* iframe_document = iframe_element->contentDocument();

  TestDelegatedInkMetadata expected_metadata(gfx::RectF(
      kIframeLeftOffset + kCanvasLeftOffset,
      kIframeTopOffset + kCanvasTopOffset, kCanvasWidth, kCanvasHeight));

  DelegatedInkTrailPresenter* presenter = CreatePresenter(
      iframe_localframe->GetDocument()->getElementById("canvas"),
      iframe_document->GetFrame());
  DCHECK(presenter);

  InkTrailStyle style;
  style.setDiameter(0.3);
  style.setColor("cyan");
  expected_metadata.SetDiameter(style.diameter());
  expected_metadata.SetColor(SK_ColorCYAN);

  gfx::PointF pt(380, 375);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(iframe_document->GetFrame()),
      CreatePointerMoveEvent(pt), &style);
  expected_metadata.SetPoint(
      gfx::PointF(pt.x() + kIframeLeftOffset, pt.y() + kIframeTopOffset));

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that values are correct when an iframe is used and presentation area
// isn't provided. Numbers and color used were chosen arbitrarily.
TEST_F(DelegatedInkTrailPresenterUnitTest, IFrameNoPresentationArea) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    iframe {
      width: 500px;
      height: 500px;
      position: fixed;
      top: 56px;
      left: 72px;
    }
    </style>
    <iframe id='iframe' src='https://example.com/iframe.html'>
    </iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    </style>
  )HTML");

  Compositor().BeginFrame();

  // When creating the expected metadata, we have to take into account the
  // offsets that are applied to the iframe, and the 2px border.
  const float kIframeBorder = 2.f;
  const float kIframeLeftOffset = 72.f + kIframeBorder;
  const float kIframeTopOffset = 56.f + kIframeBorder;
  const float kIframeHeight = 500.f;
  const float kIframeWidth = 500.f;

  Document* iframe_document =
      To<HTMLIFrameElement>(GetDocument().getElementById("iframe"))
          ->contentDocument();

  TestDelegatedInkMetadata expected_metadata(gfx::RectF(
      kIframeLeftOffset, kIframeTopOffset, kIframeWidth, kIframeHeight));

  DelegatedInkTrailPresenter* presenter =
      CreatePresenter(nullptr, iframe_document->GetFrame());
  DCHECK(presenter);

  InkTrailStyle style;
  style.setDiameter(0.01);
  style.setColor("white");
  expected_metadata.SetDiameter(style.diameter());
  expected_metadata.SetColor(SK_ColorWHITE);

  gfx::PointF pt(380, 375);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(iframe_document->GetFrame()),
      CreatePointerMoveEvent(pt), &style);
  expected_metadata.SetPoint(
      gfx::PointF(pt.x() + kIframeLeftOffset, pt.y() + kIframeTopOffset));

  expected_metadata.ExpectEqual(GetActualMetadata());
}

}  // namespace blink
