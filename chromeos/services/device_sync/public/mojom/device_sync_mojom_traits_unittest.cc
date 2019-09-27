// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/public/mojom/device_sync_mojom_traits.h"

#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DeviceSyncMojomTraitsTest, ConnectivityStatus) {
  static constexpr cryptauthv2::ConnectivityStatus kTestConnectivityStatuses[] =
      {cryptauthv2::ConnectivityStatus::ONLINE,
       cryptauthv2::ConnectivityStatus::OFFLINE};

  for (auto status_in : kTestConnectivityStatuses) {
    cryptauthv2::ConnectivityStatus status_out;

    chromeos::device_sync::mojom::ConnectivityStatus serialized_status =
        mojo::EnumTraits<chromeos::device_sync::mojom::ConnectivityStatus,
                         cryptauthv2::ConnectivityStatus>::ToMojom(status_in);
    ASSERT_TRUE((mojo::EnumTraits<
                 chromeos::device_sync::mojom::ConnectivityStatus,
                 cryptauthv2::ConnectivityStatus>::FromMojom(serialized_status,
                                                             &status_out)));
    EXPECT_EQ(status_in, status_out);
  }
}
