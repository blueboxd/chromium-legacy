// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class StyleResolverStateTest
    : public PageTestBase,
      private ScopedCSSMatchedPropertiesCacheDependenciesForTest {
 public:
  StyleResolverStateTest()
      : ScopedCSSMatchedPropertiesCacheDependenciesForTest(true) {}
};

TEST_F(StyleResolverStateTest, Dependencies) {
  StyleResolverState state(GetDocument(), *GetDocument().body(), nullptr,
                           nullptr);

  EXPECT_TRUE(state.Dependencies().IsEmpty());

  const auto& left = GetCSSPropertyLeft();
  const auto& right = GetCSSPropertyRight();
  const auto& incomparable = GetCSSPropertyInternalEmptyLineHeight();

  state.MarkDependency(left);
  EXPECT_EQ(1u, state.Dependencies().size());
  EXPECT_TRUE(state.Dependencies().Contains(left.GetCSSPropertyName()));
  EXPECT_FALSE(state.HasIncomparableDependency());

  state.MarkDependency(right);
  EXPECT_EQ(2u, state.Dependencies().size());
  EXPECT_TRUE(state.Dependencies().Contains(left.GetCSSPropertyName()));
  EXPECT_TRUE(state.Dependencies().Contains(right.GetCSSPropertyName()));
  EXPECT_FALSE(state.HasIncomparableDependency());

  state.MarkDependency(incomparable);
  EXPECT_EQ(3u, state.Dependencies().size());
  EXPECT_TRUE(state.Dependencies().Contains(left.GetCSSPropertyName()));
  EXPECT_TRUE(state.Dependencies().Contains(right.GetCSSPropertyName()));
  EXPECT_TRUE(state.Dependencies().Contains(incomparable.GetCSSPropertyName()));
  EXPECT_TRUE(state.HasIncomparableDependency());
}

TEST_F(StyleResolverStateTest, MaxDependencies) {
  StyleResolverState state(GetDocument(), *GetDocument().body(), nullptr,
                           nullptr);

  EXPECT_TRUE(state.HasValidDependencies());

  for (size_t i = 0; i < StyleResolverState::kMaxDependencies; ++i) {
    auto name = AtomicString(String::Format("--v%zu", i));
    state.MarkDependency(CustomProperty(name, GetDocument()));
    EXPECT_TRUE(state.HasValidDependencies());
  }

  state.MarkDependency(CustomProperty("--exceeds-limit", GetDocument()));
  EXPECT_FALSE(state.HasValidDependencies());
}

}  // namespace blink
