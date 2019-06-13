// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"

#include "base/stl_util.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/network_isolation_key.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojo {

TEST(NetworkIsolationKeyMojomTraitsTest, SerializeAndDeserialize) {
  std::vector<net::NetworkIsolationKey> keys = {
      net::NetworkIsolationKey(),
      net::NetworkIsolationKey(url::Origin::Create(GURL("http://a.test/")))};

  for (auto original : keys) {
    net::NetworkIsolationKey copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                network::mojom::NetworkIsolationKey>(&original, &copied));
    EXPECT_EQ(original, copied);
  }
}

}  // namespace mojo
