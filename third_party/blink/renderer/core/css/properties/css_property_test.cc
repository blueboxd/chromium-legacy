// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"

namespace blink {

TEST(CSSPropertyTest, VisitedPropertiesAreDisabled) {
  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    // "Visited" properties are internal, and should never be enabled.
    EXPECT_TRUE(!property.IsVisited() || !property.IsEnabled());
  }
}

}  // namespace blink
