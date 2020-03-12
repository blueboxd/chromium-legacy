// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include <iostream>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/web_client_hints/web_client_hints_types.mojom-shared.h"

using testing::UnorderedElementsAre;

namespace blink {

namespace mojom {

void PrintTo(const blink::mojom::WebClientHintsType& value, std::ostream* os) {
  *os << ::testing::PrintToString(static_cast<int>(value));
}

}  // namespace mojom

TEST(ClientHintsTest, SerializeLangClientHint) {
  std::string header = SerializeLangClientHint("");
  EXPECT_TRUE(header.empty());

  header = SerializeLangClientHint("es");
  EXPECT_EQ(std::string("\"es\""), header);

  header = SerializeLangClientHint("en-US,fr,de");
  EXPECT_EQ(std::string("\"en-US\", \"fr\", \"de\""), header);

  header = SerializeLangClientHint("en-US,fr,de,ko,zh-CN,ja");
  EXPECT_EQ(std::string("\"en-US\", \"fr\", \"de\", \"ko\", \"zh-CN\", \"ja\""),
            header);
}

TEST(ClientHintsTest, ParseAcceptCH) {
  base::Optional<std::vector<blink::mojom::WebClientHintsType>> result;

  // Empty is OK.
  result = ParseAcceptCH(" ",
                         /* permit_lang_hints = */ true,
                         /* permit_ua_hints = */ true);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());

  // Normal case.
  result = ParseAcceptCH("device-memory,  rtt, lang ",
                         /* permit_lang_hints = */ true,
                         /* permit_ua_hints = */ true);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(mojom::WebClientHintsType::kDeviceMemory,
                                   mojom::WebClientHintsType::kRtt,
                                   mojom::WebClientHintsType::kLang));

  // Must be a list of tokens, not other things.
  result = ParseAcceptCH("\"device-memory\", \"rtt\", \"lang\"",
                         /* permit_lang_hints = */ true,
                         /* permit_ua_hints = */ true);
  EXPECT_FALSE(result.has_value());

  // Parameters to the tokens are ignored, as encourageed by structured headers
  // spec.
  result = ParseAcceptCH("device-memory;resolution=GIB, rtt, lang",
                         /* permit_lang_hints = */ true,
                         /* permit_ua_hints = */ true);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(mojom::WebClientHintsType::kDeviceMemory,
                                   mojom::WebClientHintsType::kRtt,
                                   mojom::WebClientHintsType::kLang));

  // Unknown tokens are fine, since this meant to be extensible.
  result = ParseAcceptCH("device-memory,  rtt, lang , nosuchtokenwhywhywhy",
                         /* permit_lang_hints = */ true,
                         /* permit_ua_hints = */ true);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(mojom::WebClientHintsType::kDeviceMemory,
                                   mojom::WebClientHintsType::kRtt,
                                   mojom::WebClientHintsType::kLang));
}

TEST(ClientHintsTest, ParseAcceptCHCaseInsensitive) {
  base::Optional<std::vector<blink::mojom::WebClientHintsType>> result;

  // Matching is case-insensitive.
  result = ParseAcceptCH("Device-meMory,  Rtt, lanG ",
                         /* permit_lang_hints = */ true,
                         /* permit_ua_hints = */ true);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(mojom::WebClientHintsType::kDeviceMemory,
                                   mojom::WebClientHintsType::kRtt,
                                   mojom::WebClientHintsType::kLang));
}

// Checks to make sure that language-controlled things are filtered.
TEST(ClientHintsTest, ParseAcceptCHFlag) {
  base::Optional<std::vector<blink::mojom::WebClientHintsType>> result;

  result = ParseAcceptCH("device-memory,  rtt, lang, ua",
                         /* permit_lang_hints = */ false,
                         /* permit_ua_hints = */ true);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(mojom::WebClientHintsType::kDeviceMemory,
                                   mojom::WebClientHintsType::kRtt,
                                   mojom::WebClientHintsType::kUA));

  result = ParseAcceptCH("rtt, lang, ua, arch, platform, model, mobile",
                         /* permit_lang_hints = */ true,
                         /* permit_ua_hints = */ false);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(mojom::WebClientHintsType::kRtt,
                                   mojom::WebClientHintsType::kLang));

  result =
      ParseAcceptCH("rtt, lang, ua, ua-arch, ua-platform, ua-model, ua-mobile",
                    /* permit_lang_hints = */ true,
                    /* permit_ua_hints = */ true);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(mojom::WebClientHintsType::kRtt,
                                   mojom::WebClientHintsType::kLang,
                                   mojom::WebClientHintsType::kUA,
                                   mojom::WebClientHintsType::kUAArch,
                                   mojom::WebClientHintsType::kUAPlatform,
                                   mojom::WebClientHintsType::kUAModel,
                                   mojom::WebClientHintsType::kUAMobile));

  result = ParseAcceptCH("rtt, lang, ua, arch, platform, model, mobile",
                         /* permit_lang_hints = */ false,
                         /* permit_ua_hints = */ false);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(mojom::WebClientHintsType::kRtt));
}

}  // namespace blink
