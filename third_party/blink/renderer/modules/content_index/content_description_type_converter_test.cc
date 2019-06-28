// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_index/content_description_type_converter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom-blink.h"
#include "third_party/blink/renderer/modules/content_index/content_description.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

const blink::ContentDescription* CreateDescription(const WTF::String& category,
                                                   const WTF::String& url) {
  auto* description = blink::MakeGarbageCollected<blink::ContentDescription>();
  description->setId("id");
  description->setTitle("title");
  description->setDescription("description");
  description->setCategory(category);
  description->setIconUrl(url);
  description->setLaunchUrl(url);
  return description;
}

bool operator==(const ContentDescription& cd1, const ContentDescription& cd2) {
  return cd1.id() == cd2.id() && cd1.title() == cd2.title() &&
         cd1.description() == cd2.description() &&
         cd1.category() == cd2.category() && cd1.iconUrl() == cd2.iconUrl() &&
         cd1.launchUrl() == cd2.launchUrl();
}

TEST(ContentDescriptionConversionTest, RoundTrip) {
  auto* description = CreateDescription("homepage", "https://example.com/");
  auto mojo_description = mojom::blink::ContentDescription::From(description);
  ASSERT_TRUE(mojo_description);
  auto* round_trip_description =
      mojo_description.To<blink::ContentDescription*>();
  EXPECT_EQ(*description, *round_trip_description);
}

TEST(ContentDescriptionConversionTest, EnumRoundTrip) {
  WTF::Vector<WTF::String> categories = {"homepage", "article", "video",
                                         "audio"};
  for (const auto& category : categories) {
    auto* description = CreateDescription(category, "https://example.com/");
    auto* round_trip_description =
        mojom::blink::ContentDescription::From(description)
            .To<blink::ContentDescription*>();
    EXPECT_EQ(*description, *round_trip_description);
  }
}

}  // namespace blink
