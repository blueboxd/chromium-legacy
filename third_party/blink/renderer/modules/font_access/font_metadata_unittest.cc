// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/font_access/font_metadata.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_font_table_map.h"
#include "third_party/blink/renderer/modules/font_access/font_table_map.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

TEST(FontMetadata, TestStrings8And16Bits) {
  std::vector<FontEnumerationEntry> expectations;
#if defined(OS_MACOSX)
  expectations.push_back(FontEnumerationEntry{"Monaco", "Monaco", "Monaco"});
  expectations.push_back(
      FontEnumerationEntry{"Menlo-Regular", "Menlo Regular", "Menlo"});
  expectations.push_back(
      FontEnumerationEntry{"Menlo-Bold", "Menlo Bold", "Menlo"});
  expectations.push_back(
      FontEnumerationEntry{"Menlo-BoldItalic", "Menlo Bold Italic", "Menlo"});
#endif

  for (auto f : expectations) {
    // Set up 8 and 16 bit inputs.
    const std::string tableNameStr = "name";

    Vector<String> tableNames8;
    const String tName8 = String(tableNameStr.c_str());
    ASSERT_TRUE(tName8.Is8Bit());
    tableNames8.push_back(tName8);

    Vector<String> tableNames16;
    String tName16 = String(tableNameStr.c_str());
    tName16.Ensure16Bit();
    ASSERT_FALSE(tName16.Is8Bit());
    tableNames16.push_back(tName16);

    Vector<Vector<String>> test_inputs;
    test_inputs.push_back(tableNames8);
    test_inputs.push_back(tableNames16);

    V8TestingScope scope;
    auto* script_state = scope.GetScriptState();
    auto* metadata = FontMetadata::Create(f);
    for (auto t : test_inputs) {
      ScriptPromise promise = metadata->getTables(script_state, t);
      ScriptPromiseTester tester(script_state, promise);
      tester.WaitUntilSettled();
      EXPECT_TRUE(tester.IsFulfilled());
      ASSERT_TRUE(tester.Value().IsObject());

      FontTableMap* tableMap =
          V8FontTableMap::ToImpl(tester.Value()
                                     .V8Value()
                                     ->ToObject(script_state->GetContext())
                                     .ToLocalChecked());
      ASSERT_EQ(tableMap->size(), 1u);

      const FontTableMap::MapType map = tableMap->GetHashMap();
      EXPECT_TRUE(map.Contains("name"));
    }
  }
}

}  // namespace blink
