// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/proto_util.h"

#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/types.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

TEST(ProtoUtilTest, CreateClientInfo) {
  RequestMetadata request_metadata;
  request_metadata.chrome_info.version = base::Version({1, 2, 3, 4});
  request_metadata.chrome_info.channel = version_info::Channel::STABLE;
  request_metadata.display_metrics.density = 1;
  request_metadata.display_metrics.width_pixels = 2;
  request_metadata.display_metrics.height_pixels = 3;
  request_metadata.language_tag = "en-US";

  feedwire::ClientInfo result = CreateClientInfo(request_metadata);
  // TODO(harringtond): change back to CHROME when it is supported.
  EXPECT_EQ(feedwire::ClientInfo::TEST_APP, result.app_type());
  EXPECT_EQ(feedwire::Version::RELEASE, result.app_version().build_type());
  EXPECT_EQ(1, result.app_version().major());
  EXPECT_EQ(2, result.app_version().minor());
  EXPECT_EQ(3, result.app_version().build());
  EXPECT_EQ(4, result.app_version().revision());

  EXPECT_EQ(R"({
  screen_density: 1
  screen_width_in_pixels: 2
  screen_height_in_pixels: 3
}
)",
            ToTextProto(result.display_info(0)));
  EXPECT_EQ("en-US", result.locale());
}

}  // namespace
}  // namespace feed
