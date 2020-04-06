// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/proto_util.h"

#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/v2/types.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

TEST(ProtoUtilTest, CreateClientInfo) {
  ChromeInfo chrome_info;
  chrome_info.version = base::Version({1, 2, 3, 4});
  chrome_info.channel = version_info::Channel::STABLE;

  feedwire::ClientInfo result = CreateClientInfo(chrome_info);

  EXPECT_EQ(feedwire::ClientInfo::CHROME, result.app_type());
  EXPECT_EQ(feedwire::Version::RELEASE, result.app_version().build_type());
  EXPECT_EQ(1, result.app_version().major());
  EXPECT_EQ(2, result.app_version().minor());
  EXPECT_EQ(3, result.app_version().build());
  EXPECT_EQ(4, result.app_version().revision());
}

}  // namespace
}  // namespace feed
