// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include <iostream>

#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace blink {

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

TEST(ClientHintsTest, FilterAcceptCH) {
  EXPECT_FALSE(FilterAcceptCH(base::nullopt, true, true).has_value());

  base::Optional<std::vector<network::mojom::WebClientHintsType>> result;

  result =
      FilterAcceptCH(std::vector<network::mojom::WebClientHintsType>(
                         {network::mojom::WebClientHintsType::kDeviceMemory,
                          network::mojom::WebClientHintsType::kRtt,
                          network::mojom::WebClientHintsType::kUA}),
                     /* permit_lang_hints = */ false,
                     /* permit_ua_hints = */ true);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(
      result.value(),
      UnorderedElementsAre(network::mojom::WebClientHintsType::kDeviceMemory,
                           network::mojom::WebClientHintsType::kRtt,
                           network::mojom::WebClientHintsType::kUA));

  std::vector<network::mojom::WebClientHintsType> in{
      network::mojom::WebClientHintsType::kRtt,
      network::mojom::WebClientHintsType::kLang,
      network::mojom::WebClientHintsType::kUA,
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAPlatform,
      network::mojom::WebClientHintsType::kUAPlatformVersion,
      network::mojom::WebClientHintsType::kUAModel,
      network::mojom::WebClientHintsType::kUAMobile,
      network::mojom::WebClientHintsType::kUAFullVersion};

  result = FilterAcceptCH(in,
                          /* permit_lang_hints = */ true,
                          /* permit_ua_hints = */ false);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(network::mojom::WebClientHintsType::kRtt,
                                   network::mojom::WebClientHintsType::kLang));

  result = FilterAcceptCH(in,
                          /* permit_lang_hints = */ true,
                          /* permit_ua_hints = */ true);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(
                  network::mojom::WebClientHintsType::kRtt,
                  network::mojom::WebClientHintsType::kLang,
                  network::mojom::WebClientHintsType::kUA,
                  network::mojom::WebClientHintsType::kUAArch,
                  network::mojom::WebClientHintsType::kUAPlatform,
                  network::mojom::WebClientHintsType::kUAPlatformVersion,
                  network::mojom::WebClientHintsType::kUAModel,
                  network::mojom::WebClientHintsType::kUAMobile,
                  network::mojom::WebClientHintsType::kUAFullVersion));

  result = FilterAcceptCH(in,
                          /* permit_lang_hints = */ false,
                          /* permit_ua_hints = */ false);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(network::mojom::WebClientHintsType::kRtt));
}

}  // namespace blink
