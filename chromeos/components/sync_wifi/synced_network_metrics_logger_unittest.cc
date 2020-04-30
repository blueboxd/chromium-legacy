// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/synced_network_metrics_logger.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/components/sync_wifi/network_test_helper.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace sync_wifi {

namespace {

const char kFailureReasonAllHistogram[] =
    "Network.Wifi.Synced.Connection.FailureReason";
const char kConnectionResultAllHistogram[] =
    "Network.Wifi.Synced.Connection.Result";

const char kFailureReasonManualHistogram[] =
    "Network.Wifi.Synced.ManualConnection.FailureReason";
const char kConnectionResultManualHistogram[] =
    "Network.Wifi.Synced.ManualConnection.Result";

}  // namespace

class SyncedNetworkMetricsLoggerTest : public testing::Test {
 public:
  SyncedNetworkMetricsLoggerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    network_test_helper_ = std::make_unique<NetworkTestHelper>();
  }
  ~SyncedNetworkMetricsLoggerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    network_test_helper_->SetUp();
    base::RunLoop().RunUntilIdle();

    synced_network_metrics_logger_.reset(new SyncedNetworkMetricsLogger(
        network_test_helper_->network_state_test_helper()
            ->network_state_handler(),
        /* network_connection_handler */ nullptr));
  }

  void TearDown() override {
    chromeos::NetworkHandler::Shutdown();
    testing::Test::TearDown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  SyncedNetworkMetricsLogger* synced_network_metrics_logger() const {
    return synced_network_metrics_logger_.get();
  }

  NetworkTestHelper* network_test_helper() const {
    return network_test_helper_.get();
  }

  void SetNetworkProperty(const std::string& service_path,
                          const std::string& key,
                          const std::string& value) {
    network_test_helper_->network_state_test_helper()->SetServiceProperty(
        service_path, key, base::Value(value));
  }

  const NetworkState* CreateNetwork(bool from_sync) {
    std::string guid = network_test_helper()->ConfigureWiFiNetwork(
        "ssid", /*is_secure=*/true, /*in_profile=*/true, /*has_connected=*/true,
        /*owned_by_user=*/true, /*configured_by_sync=*/from_sync);
    return network_test_helper()
        ->network_state_helper()
        .network_state_handler()
        ->GetNetworkStateFromGuid(guid);
  }

 private:
  std::unique_ptr<NetworkTestHelper> network_test_helper_;
  std::unique_ptr<SyncedNetworkMetricsLogger> synced_network_metrics_logger_;

  DISALLOW_COPY_AND_ASSIGN(SyncedNetworkMetricsLoggerTest);
};

TEST_F(SyncedNetworkMetricsLoggerTest,
       SuccessfulManualConnection_SyncedNetwork) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/true);

  synced_network_metrics_logger()->ConnectSucceeded(network->path());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(kConnectionResultManualHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kFailureReasonManualHistogram, 0);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       SuccessfulManualConnection_LocallyConfiguredNetwork) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/false);

  synced_network_metrics_logger()->ConnectSucceeded(network->path());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(kConnectionResultManualHistogram, 0);
  histogram_tester.ExpectTotalCount(kFailureReasonManualHistogram, 0);
}

TEST_F(SyncedNetworkMetricsLoggerTest, FailedManualConnection_SyncedNetwork) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/true);

  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateFailure);
  SetNetworkProperty(network->path(), shill::kErrorProperty,
                     shill::kErrorBadPassphrase);
  synced_network_metrics_logger()->ConnectFailed(
      network->path(), NetworkConnectionHandler::kErrorConnectFailed);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(kConnectionResultManualHistogram, false,
                                     1);
  histogram_tester.ExpectBucketCount(
      kFailureReasonManualHistogram,
      SyncedNetworkMetricsLogger::ConnectionFailureReason::kBadPassphrase, 1);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       FailedManualConnection_LocallyConfiguredNetwork) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/false);

  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateFailure);
  SetNetworkProperty(network->path(), shill::kErrorProperty,
                     shill::kErrorBadPassphrase);
  synced_network_metrics_logger()->ConnectFailed(
      network->path(), NetworkConnectionHandler::kErrorConnectFailed);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(kConnectionResultManualHistogram, 0);
  histogram_tester.ExpectTotalCount(kFailureReasonManualHistogram, 0);
}

TEST_F(SyncedNetworkMetricsLoggerTest, FailedConnection_SyncedNetwork) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/true);

  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateConfiguration);
  synced_network_metrics_logger()->NetworkConnectionStateChanged(network);

  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateFailure);
  SetNetworkProperty(network->path(), shill::kErrorProperty,
                     shill::kErrorUnknownFailure);
  base::RunLoop().RunUntilIdle();

  synced_network_metrics_logger()->NetworkConnectionStateChanged(network);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(kConnectionResultAllHistogram, false, 1);
  histogram_tester.ExpectBucketCount(
      kFailureReasonAllHistogram,
      SyncedNetworkMetricsLogger::ConnectionFailureReason::kUnknown, 1);
}

TEST_F(SyncedNetworkMetricsLoggerTest,
       FailedConnection_LocallyConfiguredNetwork) {
  base::HistogramTester histogram_tester;
  const NetworkState* network = CreateNetwork(/*from_sync=*/false);

  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateConfiguration);
  synced_network_metrics_logger()->NetworkConnectionStateChanged(network);

  SetNetworkProperty(network->path(), shill::kStateProperty,
                     shill::kStateFailure);
  SetNetworkProperty(network->path(), shill::kErrorProperty,
                     shill::kErrorBadPassphrase);
  base::RunLoop().RunUntilIdle();
  synced_network_metrics_logger()->NetworkConnectionStateChanged(network);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(kConnectionResultAllHistogram, 0);
  histogram_tester.ExpectTotalCount(kFailureReasonAllHistogram, 0);
}

}  // namespace sync_wifi

}  // namespace chromeos
