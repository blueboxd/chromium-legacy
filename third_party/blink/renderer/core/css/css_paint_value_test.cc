// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_paint_value.h"

#include <memory>
#include "base/auto_reset.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/paint_generated_image.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::Values;

namespace blink {
namespace {

class CSSPaintValueTest : public RenderingTest,
                          public ::testing::WithParamInterface<bool> {
 public:
  CSSPaintValueTest() : scoped_off_main_thread_css_paint_(GetParam()) {}

 private:
  ScopedOffMainThreadCSSPaintForTest scoped_off_main_thread_css_paint_;
};

INSTANTIATE_TEST_SUITE_P(, CSSPaintValueTest, Values(false, true));

class MockCSSPaintImageGenerator : public CSSPaintImageGenerator {
 public:
  MockCSSPaintImageGenerator() {
    // These methods return references, so setup a default ON_CALL to make them
    // easier to use. They can be overridden by a specific test if desired.
    ON_CALL(*this, NativeInvalidationProperties())
        .WillByDefault(ReturnRef(native_properties_));
    ON_CALL(*this, CustomInvalidationProperties())
        .WillByDefault(ReturnRef(custom_properties_));
    ON_CALL(*this, InputArgumentTypes())
        .WillByDefault(ReturnRef(input_argument_types_));
  }

  MOCK_METHOD3(Paint,
               scoped_refptr<Image>(const ImageResourceObserver&,
                                    const FloatSize& container_size,
                                    const CSSStyleValueVector*));
  MOCK_CONST_METHOD0(NativeInvalidationProperties, Vector<CSSPropertyID>&());
  MOCK_CONST_METHOD0(CustomInvalidationProperties, Vector<AtomicString>&());
  MOCK_CONST_METHOD0(HasAlpha, bool());
  MOCK_CONST_METHOD0(InputArgumentTypes, Vector<CSSSyntaxDescriptor>&());
  MOCK_CONST_METHOD0(IsImageGeneratorReady, bool());
  MOCK_CONST_METHOD0(WorkletId, int());

 private:
  Vector<CSSPropertyID> native_properties_;
  Vector<AtomicString> custom_properties_;
  Vector<CSSSyntaxDescriptor> input_argument_types_;
};

// CSSPaintImageGenerator requires that CSSPaintImageGeneratorCreateFunction be
// a static method. As such, it cannot access a class member and so instead we
// store a pointer to the overriding generator globally.
MockCSSPaintImageGenerator* g_override_generator = nullptr;
CSSPaintImageGenerator* ProvideOverrideGenerator(
    const String&,
    const Document&,
    CSSPaintImageGenerator::Observer*) {
  return g_override_generator;
}
}  // namespace

TEST_P(CSSPaintValueTest, DelayPaintUntilGeneratorReady) {
  NiceMock<MockCSSPaintImageGenerator>* mock_generator =
      MakeGarbageCollected<NiceMock<MockCSSPaintImageGenerator>>();
  base::AutoReset<MockCSSPaintImageGenerator*> scoped_override_generator(
      &g_override_generator, mock_generator);
  base::AutoReset<CSSPaintImageGenerator::CSSPaintImageGeneratorCreateFunction>
      scoped_create_function(
          CSSPaintImageGenerator::GetCreateFunctionForTesting(),
          ProvideOverrideGenerator);

  const FloatSize target_size(100, 100);
  if (RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled()) {
    // In off-thread CSS Paint, the actual paint call is deferred.
    EXPECT_CALL(*mock_generator, Paint(_, _, _)).Times(0);
  } else {
    ON_CALL(*mock_generator, Paint(_, _, _))
        .WillByDefault(
            Return(PaintGeneratedImage::Create(nullptr, target_size)));
  }

  SetBodyInnerHTML(R"HTML(
    <div id="target"></div>
  )HTML");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ComputedStyle& style = *target->Style();

  auto* ident = MakeGarbageCollected<CSSCustomIdentValue>("testpainter");
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident);

  // Initially the generator is not ready, so GetImage should fail.
  EXPECT_FALSE(
      paint_value->GetImage(*target, GetDocument(), style, target_size));

  // Now mark the generator as ready - GetImage should then succeed.
  EXPECT_CALL(*mock_generator, IsImageGeneratorReady()).WillOnce(Return(true));
  EXPECT_TRUE(
      paint_value->GetImage(*target, GetDocument(), style, target_size));
}

// Regression test for https://crbug.com/835589.
TEST_P(CSSPaintValueTest, DoNotPaintForLink) {
  SetBodyInnerHTML(R"HTML(
    <style>
      a {
        background-image: paint(linkpainter);
        width: 100px;
        height: 100px;
      }
    </style>
    <a href="http://www.example.com" id="target"></a>
  )HTML");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ComputedStyle& style = *target->Style();
  ASSERT_NE(style.InsideLink(), EInsideLink::kNotInsideLink);

  auto* ident = MakeGarbageCollected<CSSCustomIdentValue>("linkpainter");
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident);
  EXPECT_FALSE(paint_value->GetImage(*target, GetDocument(), style,
                                     FloatSize(100, 100)));
}

// Regression test for https://crbug.com/835589.
TEST_P(CSSPaintValueTest, DoNotPaintWhenAncestorHasLink) {
  SetBodyInnerHTML(R"HTML(
    <style>
      a {
        width: 200px;
        height: 200px;
      }
      b {
        background-image: paint(linkpainter);
        width: 100px;
        height: 100px;
      }
    </style>
    <a href="http://www.example.com" id="ancestor">
      <b id="target"></b>
    </a>
  )HTML");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ComputedStyle& style = *target->Style();
  ASSERT_NE(style.InsideLink(), EInsideLink::kNotInsideLink);

  auto* ident = MakeGarbageCollected<CSSCustomIdentValue>("linkpainter");
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident);
  EXPECT_FALSE(paint_value->GetImage(*target, GetDocument(), style,
                                     FloatSize(100, 100)));
}

TEST_P(CSSPaintValueTest, BuildInputArgumentValuesNotCrash) {
  // The BuildInputArgumentValues is called when the flag is enabled only.
  if (!RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <div id="target"></div>
  )HTML");

  auto* ident = MakeGarbageCollected<CSSCustomIdentValue>("testpainter");
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident);
  EXPECT_NE(paint_value, nullptr);

  EXPECT_EQ(paint_value->GetParsedInputArgumentsForTesting(), nullptr);
  Vector<std::unique_ptr<CrossThreadStyleValue>> cross_thread_input_arguments;
  paint_value->BuildInputArgumentValuesForTesting(cross_thread_input_arguments);
  EXPECT_EQ(cross_thread_input_arguments.size(), 0u);
}

}  // namespace blink
