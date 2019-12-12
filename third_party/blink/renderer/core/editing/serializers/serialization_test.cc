// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/serializers/serialization.h"

#include <gtest/gtest.h>

namespace blink {
namespace {

// Regression test for https://crbug.com/1032673
TEST(SerializationTest, CantCreateFragmentCrash) {
  // CreateFragmentFromMarkupWithContext() fails to create a fragment for the
  // following markup. Should return an empty string as the sanitized markup
  // instead of crashing.
  const String html =
      "<article><dcell></dcell>A<td><dcol></"
      "dcol>A0<td>&percnt;&lbrack;<command></"
      "command><img>0AA00A0AAAAAAA00A<optgroup>&NotLess;&Eacute;&andand;&"
      "Uarrocir;&jfr;&esim;&Alpha;&angmsdab;&ogt;&lesseqqgtr;&vBar;&plankv;&"
      "curlywedge;&lcedil;&Mfr;&Barwed;&rlm;<kbd><animateColor></"
      "animateColor>A000AA0AA000A0<plaintext></"
      "plaintext><title>0A0AA00A0A0AA000A<switch><img "
      "src=\"../resources/abe.png\"> zz";
  const String sanitized = SanitizeMarkupWithContext(html, 0, html.length());
  EXPECT_TRUE(sanitized.IsEmpty());
}

// Regression test for https://crbug.com/1032389
TEST(SerializationTest, SVGForeignObjectCrash) {
  const String markup =
      "<svg>"
      "  <foreignObject>"
      "    <br>"
      "    <div style=\"height: 50px;\"></div>"
      "  </foreignObject>"
      "</svg>"
      "<span>\u00A0</span>";
  const String sanitized =
      SanitizeMarkupWithContext(markup, 0, markup.length());
  // This is a crash test. We don't verify the content of the sanitized markup
  // as it's too verbose and not interesting.
  EXPECT_FALSE(sanitized.IsEmpty());
}

}  // namespace
}  // namespace blink
