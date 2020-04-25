// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/scoped_blink_ax_event_intent.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink {

using ScopedBlinkAXEventIntentTest = RenderingTest;

TEST_F(ScopedBlinkAXEventIntentTest, SingleIntent) {
  AXContext ax_context(GetDocument());
  AXObjectCache* cache = GetDocument().ExistingAXObjectCache();
  ASSERT_NE(nullptr, cache);

  {
    ScopedBlinkAXEventIntent scoped_intent(
        {ax::mojom::blink::Command::kCut,
         ax::mojom::blink::TextBoundary::kWordEnd,
         ax::mojom::blink::MoveDirection::kForward},
        &GetDocument());

    EXPECT_TRUE(cache->ActiveEventIntents().Contains(scoped_intent.intent()));
    EXPECT_EQ(1u, cache->ActiveEventIntents().size());
  }

  EXPECT_TRUE(cache->ActiveEventIntents().IsEmpty());
}

TEST_F(ScopedBlinkAXEventIntentTest, NestedIntents) {
  AXContext ax_context(GetDocument());
  AXObjectCache* cache = GetDocument().ExistingAXObjectCache();
  ASSERT_NE(nullptr, cache);

  {
    ScopedBlinkAXEventIntent scoped_intent1(
        {ax::mojom::blink::Command::kType,
         ax::mojom::blink::TextBoundary::kCharacter,
         ax::mojom::blink::MoveDirection::kForward},
        &GetDocument());

    {
      ScopedBlinkAXEventIntent scoped_intent2(
          {ax::mojom::blink::Command::kCut,
           ax::mojom::blink::TextBoundary::kWordEnd,
           ax::mojom::blink::MoveDirection::kForward},
          &GetDocument());

      EXPECT_TRUE(
          cache->ActiveEventIntents().Contains(scoped_intent1.intent()));
      EXPECT_TRUE(
          cache->ActiveEventIntents().Contains(scoped_intent2.intent()));
      EXPECT_EQ(1u, cache->ActiveEventIntents().count(scoped_intent1.intent()));
      EXPECT_EQ(1u, cache->ActiveEventIntents().count(scoped_intent2.intent()));
      EXPECT_EQ(2u, cache->ActiveEventIntents().size());
    }

    EXPECT_TRUE(cache->ActiveEventIntents().Contains(scoped_intent1.intent()));
    EXPECT_EQ(1u, cache->ActiveEventIntents().count(scoped_intent1.intent()));
    EXPECT_EQ(1u, cache->ActiveEventIntents().size());
  }

  EXPECT_TRUE(cache->ActiveEventIntents().IsEmpty());
}

TEST_F(ScopedBlinkAXEventIntentTest, NestedSameIntents) {
  AXContext ax_context(GetDocument());
  AXObjectCache* cache = GetDocument().ExistingAXObjectCache();
  ASSERT_NE(nullptr, cache);

  {
    ScopedBlinkAXEventIntent scoped_intent1(
        {ax::mojom::blink::Command::kType,
         ax::mojom::blink::TextBoundary::kCharacter,
         ax::mojom::blink::MoveDirection::kForward},
        &GetDocument());

    {
      // Create a second, identical intent.
      ScopedBlinkAXEventIntent scoped_intent2(
          {ax::mojom::blink::Command::kType,
           ax::mojom::blink::TextBoundary::kCharacter,
           ax::mojom::blink::MoveDirection::kForward},
          &GetDocument());

      EXPECT_TRUE(
          cache->ActiveEventIntents().Contains(scoped_intent1.intent()));
      EXPECT_EQ(2u, cache->ActiveEventIntents().count(scoped_intent1.intent()));
      EXPECT_EQ(1u, cache->ActiveEventIntents().size());
    }

    EXPECT_TRUE(cache->ActiveEventIntents().Contains(scoped_intent1.intent()));
    EXPECT_EQ(1u, cache->ActiveEventIntents().count(scoped_intent1.intent()));
    EXPECT_EQ(1u, cache->ActiveEventIntents().size());
  }

  EXPECT_TRUE(cache->ActiveEventIntents().IsEmpty());
}

}  // namespace blink
