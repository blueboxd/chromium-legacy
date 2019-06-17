// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_style_property_map.h"

#include <memory>
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class PaintWorkletStylePropertyMapTest : public PageTestBase {
 public:
  PaintWorkletStylePropertyMapTest() = default;

  void SetUp() override { PageTestBase::SetUp(IntSize()); }

  Node* PageNode() { return GetDocument().documentElement(); }

  void ShutDown(base::WaitableEvent* waitable_event) {
    DCHECK(!IsMainThread());
    waitable_event->Signal();
  }

  void ShutDownThread() {
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&PaintWorkletStylePropertyMapTest::ShutDown,
                            CrossThreadUnretained(this),
                            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void CheckCustomProperties(const PaintWorkletStylePropertyMap* map,
                             DummyExceptionStateForTesting& exception_state) {
    const CSSStyleValue* foo = map->get(nullptr, "--foo", exception_state);
    ASSERT_NE(nullptr, foo);
    ASSERT_EQ(CSSStyleValue::kUnknownType, foo->GetType());
    EXPECT_FALSE(exception_state.HadException());

    EXPECT_EQ(true, map->has(nullptr, "--foo", exception_state));
    EXPECT_FALSE(exception_state.HadException());

    CSSStyleValueVector fooAll = map->getAll(nullptr, "--foo", exception_state);
    EXPECT_EQ(1U, fooAll.size());
    ASSERT_NE(nullptr, fooAll[0]);
    ASSERT_EQ(CSSStyleValue::kUnknownType, fooAll[0]->GetType());
    EXPECT_FALSE(exception_state.HadException());

    EXPECT_EQ(nullptr, map->get(nullptr, "--quix", exception_state));
    EXPECT_FALSE(exception_state.HadException());

    EXPECT_EQ(false, map->has(nullptr, "--quix", exception_state));
    EXPECT_FALSE(exception_state.HadException());

    EXPECT_EQ(CSSStyleValueVector(),
              map->getAll(nullptr, "--quix", exception_state));
    EXPECT_FALSE(exception_state.HadException());
  }

  void CheckNativeProperties(const PaintWorkletStylePropertyMap* map,
                             DummyExceptionStateForTesting& exception_state) {
    map->get(nullptr, "color", exception_state);
    EXPECT_FALSE(exception_state.HadException());

    map->has(nullptr, "color", exception_state);
    EXPECT_FALSE(exception_state.HadException());

    map->getAll(nullptr, "color", exception_state);
    EXPECT_FALSE(exception_state.HadException());

    map->get(nullptr, "align-contents", exception_state);
    EXPECT_TRUE(exception_state.HadException());
    exception_state.ClearException();

    map->has(nullptr, "align-contents", exception_state);
    EXPECT_TRUE(exception_state.HadException());
    exception_state.ClearException();

    map->getAll(nullptr, "align-contents", exception_state);
    EXPECT_TRUE(exception_state.HadException());
    exception_state.ClearException();
  }

  void CheckStyleMap(base::WaitableEvent* waitable_event,
                     scoped_refptr<PaintWorkletInput> input) {
    DCHECK(!IsMainThread());

    PaintWorkletStylePropertyMap* map =
        MakeGarbageCollected<PaintWorkletStylePropertyMap>(
            input->StyleMapData());
    DCHECK(map);
    DummyExceptionStateForTesting exception_state;
    CheckNativeProperties(map, exception_state);
    CheckCustomProperties(map, exception_state);

    const PaintWorkletStylePropertyMap::CrossThreadData& data =
        map->StyleMapDataForTest();
    EXPECT_EQ(data.size(), 8u);
    EXPECT_EQ(data.at("color")->ToCSSStyleValue()->CSSText(), "rgb(0, 255, 0)");
    EXPECT_EQ(data.at("color")->ToCSSStyleValue()->GetType(),
              CSSStyleValue::StyleValueType::kUnknownType);
    EXPECT_EQ(data.at("align-items")->ToCSSStyleValue()->CSSText(), "normal");
    EXPECT_EQ(data.at("align-items")->ToCSSStyleValue()->GetType(),
              CSSStyleValue::StyleValueType::kUnknownType);
    EXPECT_EQ(
        static_cast<CSSKeywordValue*>(data.at("display")->ToCSSStyleValue())
            ->value(),
        "block");
    EXPECT_EQ(data.at("display")->ToCSSStyleValue()->GetType(),
              CSSStyleValue::StyleValueType::kKeywordType);
    EXPECT_EQ(data.at("--foo")->ToCSSStyleValue()->CSSText(), "PaintWorklet");
    EXPECT_EQ(data.at("--foo")->ToCSSStyleValue()->GetType(),
              CSSStyleValue::StyleValueType::kUnknownType);
    EXPECT_EQ(data.at("--bar")->ToCSSStyleValue()->CSSText(), "");
    EXPECT_EQ(data.at("--bar")->ToCSSStyleValue()->GetType(),
              CSSStyleValue::StyleValueType::kUnknownType);
    EXPECT_EQ(data.at("--keyword")->ToCSSStyleValue()->GetType(),
              CSSStyleValue::StyleValueType::kKeywordType);
    EXPECT_EQ(
        To<CSSKeywordValue>(data.at("--keyword")->ToCSSStyleValue())->value(),
        "test");
    EXPECT_EQ(data.at("--x")->ToCSSStyleValue()->GetType(),
              CSSStyleValue::StyleValueType::kUnitType);
    EXPECT_EQ(To<CSSUnitValue>(data.at("--x")->ToCSSStyleValue())->value(), 10);
    EXPECT_EQ(To<CSSUnitValue>(data.at("--x")->ToCSSStyleValue())->unit(),
              "px");
    EXPECT_EQ(data.at("--y")->ToCSSStyleValue()->GetType(),
              CSSStyleValue::StyleValueType::kUnknownType);
    EXPECT_EQ(data.at("--y")->ToCSSStyleValue()->CSSText(), "rgb(0, 255, 0)");

    waitable_event->Signal();
  }

 protected:
  std::unique_ptr<blink::Thread> thread_;
};

// This test ensures that Blink::PaintWorkletInput can be safely passed cross
// threads and no information is lost.
TEST_F(PaintWorkletStylePropertyMapTest, PassValuesCrossThread) {
  Vector<CSSPropertyID> native_properties({CSSPropertyID::kColor,
                                           CSSPropertyID::kAlignItems,
                                           CSSPropertyID::kDisplay});
  GetDocument().documentElement()->style()->setProperty(
      &GetDocument(), "color", "rgb(0, 255, 0)", "", ASSERT_NO_EXCEPTION);
  GetDocument().documentElement()->style()->setProperty(
      &GetDocument(), "display", "block", "", ASSERT_NO_EXCEPTION);
  Vector<AtomicString> custom_properties(
      {"--foo", "--bar", "--keyword", "--x", "--y"});
  css_test_helpers::RegisterProperty(GetDocument(), "--keyword", "test", "test",
                                     false);
  css_test_helpers::RegisterProperty(GetDocument(), "--x", "<length>", "42px",
                                     false);
  css_test_helpers::RegisterProperty(GetDocument(), "--y", "<color>",
                                     "rgb(0, 0, 0)", false);
  GetDocument().documentElement()->style()->setProperty(
      &GetDocument(), "--foo", "PaintWorklet", "", ASSERT_NO_EXCEPTION);
  GetDocument().documentElement()->style()->setProperty(
      &GetDocument(), "--keyword", "test", "", ASSERT_NO_EXCEPTION);
  GetDocument().documentElement()->style()->setProperty(
      &GetDocument(), "--x", "10px", "", ASSERT_NO_EXCEPTION);
  GetDocument().documentElement()->style()->setProperty(
      &GetDocument(), "--y", "rgb(0, 255, 0)", "", ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesForTest();
  Node* node = PageNode();

  Vector<std::unique_ptr<CrossThreadStyleValue>> input_arguments;
  PaintWorkletStylePropertyMap::CrossThreadData data =
      PaintWorkletStylePropertyMap::BuildCrossThreadData(
          GetDocument(), node->ComputedStyleRef(), node, native_properties,
          custom_properties);
  scoped_refptr<PaintWorkletInput> input =
      base::MakeRefCounted<PaintWorkletInput>("test", FloatSize(100, 100), 1.0f,
                                              1, std::move(data),
                                              std::move(input_arguments));
  DCHECK(input);

  thread_ = blink::Thread::CreateThread(
      ThreadCreationParams(WebThreadType::kTestThread).SetSupportsGC(true));
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&PaintWorkletStylePropertyMapTest::CheckStyleMap,
                          CrossThreadUnretained(this),
                          CrossThreadUnretained(&waitable_event),
                          std::move(input)));
  waitable_event.Wait();

  ShutDownThread();
}

}  // namespace blink
