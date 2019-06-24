// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace chromeos {
namespace cros_healthd {
namespace {

std::vector<mojom::NonRemovableBlockDeviceInfoPtr>
MakeNonRemovableBlockDeviceInfo() {
  std::vector<mojom::NonRemovableBlockDeviceInfoPtr> info;
  info.push_back(mojom::NonRemovableBlockDeviceInfo::New(
      "test_path", 123 /* size */, "test_type", 10 /* manfid */, "test_name",
      768 /* serial */));
  info.push_back(mojom::NonRemovableBlockDeviceInfo::New(
      "test_path2", 124 /* size */, "test_type2", 11 /* manfid */, "test_name2",
      767 /* serial */));
  return info;
}

class MockCrosHealthdService : public mojom::CrosHealthdService {
 public:
  mojo::PendingRemote<mojom::CrosHealthdService> GetPendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD1(ProbeNonRemovableBlockDeviceInfo,
               void(ProbeNonRemovableBlockDeviceInfoCallback callback));

 private:
  mojo::Receiver<mojom::CrosHealthdService> receiver_{this};
};

class CrosHealthdServiceConnectionTest : public testing::Test {
 public:
  CrosHealthdServiceConnectionTest() = default;

  void SetUp() override {
    CrosHealthdClient::InitializeFakeWithMockService(
        mock_service_.GetPendingRemote());
  }

  void TearDown() override { CrosHealthdClient::Shutdown(); }

  MockCrosHealthdService* mock_service() { return &mock_service_; }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  StrictMock<MockCrosHealthdService> mock_service_;

  DISALLOW_COPY_AND_ASSIGN(CrosHealthdServiceConnectionTest);
};

TEST_F(CrosHealthdServiceConnectionTest, ProbeNonRemovableBlockDeviceInfo) {
  EXPECT_CALL(*mock_service(), ProbeNonRemovableBlockDeviceInfo(_))
      .WillOnce(WithArgs<0>(Invoke(
          [](mojom::CrosHealthdService::ProbeNonRemovableBlockDeviceInfoCallback
                 callback) {
            std::move(callback).Run(MakeNonRemovableBlockDeviceInfo());
          })));
  bool callback_done = false;
  ServiceConnection::GetInstance()->ProbeNonRemovableBlockDeviceInfo(
      base::BindOnce(
          [](bool* callback_done,
             std::vector<mojom::NonRemovableBlockDeviceInfoPtr> info) {
            EXPECT_EQ(info, MakeNonRemovableBlockDeviceInfo());
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

}  // namespace
}  // namespace cros_healthd
}  // namespace chromeos
