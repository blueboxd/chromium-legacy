// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/origin_isolation_parser.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(OriginIsolationHeaderTest, Parse) {
  EXPECT_EQ(ParseOriginIsolation(""), false);

  EXPECT_EQ(ParseOriginIsolation("?1"), true);
  EXPECT_EQ(ParseOriginIsolation("?0"), false);

  EXPECT_EQ(ParseOriginIsolation("?1;param"), true);
  EXPECT_EQ(ParseOriginIsolation("?1;param=value"), true);
  EXPECT_EQ(ParseOriginIsolation("?1;param=value;param2=value2"), true);

  EXPECT_EQ(ParseOriginIsolation("true"), false);
  EXPECT_EQ(ParseOriginIsolation("\"?1\""), false);
  EXPECT_EQ(ParseOriginIsolation("1"), false);
  EXPECT_EQ(ParseOriginIsolation("?2"), false);
  EXPECT_EQ(ParseOriginIsolation("(?1)"), false);
}

}  // namespace network
