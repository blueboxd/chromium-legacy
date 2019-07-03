// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

static const int kWidth = 50;
static const int kHeight = 75;
static const float kZoom = 1.0;
static const float kDeviceScaleFactor = 1.23;

class PaintRenderingContext2DTest : public testing::Test {
 protected:
  void SetUp() override;

  Persistent<PaintRenderingContext2D> ctx_;
};

void PaintRenderingContext2DTest::SetUp() {
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  context_settings->setAlpha(false);
  ctx_ = MakeGarbageCollected<PaintRenderingContext2D>(
      IntSize(kWidth, kHeight), context_settings, kZoom, kDeviceScaleFactor);
}

void TrySettingStrokeStyle(PaintRenderingContext2D* ctx,
                           const String& expected,
                           const String& value) {
  StringOrCanvasGradientOrCanvasPattern result, arg, dummy;
  dummy.SetString("red");
  arg.SetString(value);
  ctx->setStrokeStyle(dummy);
  ctx->setStrokeStyle(arg);
  ctx->strokeStyle(result);
  EXPECT_EQ(expected, result.GetAsString());
}

TEST_F(PaintRenderingContext2DTest, testParseColorOrCurrentColor) {
  TrySettingStrokeStyle(ctx_.Get(), "#0000ff", "blue");
  TrySettingStrokeStyle(ctx_.Get(), "#000000", "currentColor");
}

TEST_F(PaintRenderingContext2DTest, testWidthAndHeight) {
  EXPECT_EQ(kWidth, ctx_->Width());
  EXPECT_EQ(kHeight, ctx_->Height());
}

TEST_F(PaintRenderingContext2DTest, testBasicState) {
  const double kShadowBlurBefore = 2;
  const double kShadowBlurAfter = 3;

  const String line_join_before = "bevel";
  const String line_join_after = "round";

  ctx_->setShadowBlur(kShadowBlurBefore);
  ctx_->setLineJoin(line_join_before);
  EXPECT_EQ(kShadowBlurBefore, ctx_->shadowBlur());
  EXPECT_EQ(line_join_before, ctx_->lineJoin());

  ctx_->save();

  ctx_->setShadowBlur(kShadowBlurAfter);
  ctx_->setLineJoin(line_join_after);
  EXPECT_EQ(kShadowBlurAfter, ctx_->shadowBlur());
  EXPECT_EQ(line_join_after, ctx_->lineJoin());

  ctx_->restore();

  EXPECT_EQ(kShadowBlurBefore, ctx_->shadowBlur());
  EXPECT_EQ(line_join_before, ctx_->lineJoin());
}

TEST_F(PaintRenderingContext2DTest, setTransformWithDeviceScaleFactor) {
  DOMMatrix* matrix = ctx_->getTransform();
  EXPECT_TRUE(matrix->isIdentity());
  ctx_->setTransform(2.1, 2.5, 1.4, 2.3, 20, 50);
  matrix = ctx_->getTransform();
  double epsilon = 0.000001;
  EXPECT_NEAR(matrix->a(), 2.1 / kDeviceScaleFactor, epsilon);
  EXPECT_NEAR(matrix->b(), 2.5 / kDeviceScaleFactor, epsilon);
  EXPECT_NEAR(matrix->c(), 1.4 / kDeviceScaleFactor, epsilon);
  EXPECT_NEAR(matrix->d(), 2.3 / kDeviceScaleFactor, epsilon);
  EXPECT_NEAR(matrix->e(), 20 / kDeviceScaleFactor, epsilon);
  EXPECT_NEAR(matrix->f(), 50 / kDeviceScaleFactor, epsilon);
}

TEST_F(PaintRenderingContext2DTest, setTransformWithDefaultDeviceScaleFactor) {
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  PaintRenderingContext2D* ctx = MakeGarbageCollected<PaintRenderingContext2D>(
      IntSize(kWidth, kHeight), context_settings, kZoom,
      1.0 /* device_scale_factor */);
  DOMMatrix* matrix = ctx->getTransform();
  EXPECT_TRUE(matrix->isIdentity());
  ctx->setTransform(1.2, 2.3, 3.4, 4.5, 56, 67);
  matrix = ctx->getTransform();
  EXPECT_FLOAT_EQ(matrix->a(), 1.2);
  EXPECT_FLOAT_EQ(matrix->b(), 2.3);
  EXPECT_FLOAT_EQ(matrix->c(), 3.4);
  EXPECT_FLOAT_EQ(matrix->d(), 4.5);
  EXPECT_FLOAT_EQ(matrix->e(), 56);
  EXPECT_FLOAT_EQ(matrix->f(), 67);
}

}  // namespace
}  // namespace blink
