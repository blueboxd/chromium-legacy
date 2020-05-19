// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_measurement.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "net/base/network_change_notifier.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace data_use_measurement {

class DataUseMeasurementTest {
 public:
  DataUseMeasurementTest()
      : data_use_measurement_(
            nullptr,
            network::TestNetworkConnectionTracker::GetInstance()) {
    // During the test it is expected to not have cellular connection.
    DCHECK(!net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType()));
  }

  // This function makes a user request and confirms that its effect is
  // reflected in proper histograms.
  void TestForAUserRequest(const std::string& target_dimension) {
    base::HistogramTester histogram_tester;
    data_use_measurement_.RecordDownstreamUserTrafficSizeMetric(
        true /* is_tab_visible */, 5 /* bytest */);
    data_use_measurement_.RecordDownstreamUserTrafficSizeMetric(
        true /* is_tab_visible */, 5 /* bytest */);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.User.Downstream." +
                                          target_dimension + kConnectionType,
                                      2);
  }


  DataUseMeasurement* data_use_measurement() { return &data_use_measurement_; }

 protected:
  DataUseMeasurement data_use_measurement_;
  const std::string kConnectionType = "NotCellular";

  DISALLOW_COPY_AND_ASSIGN(DataUseMeasurementTest);
};

// This test function tests recording of data use information in UMA histogram
// when packet is originated from user or services when the app is in the
// foreground or the OS is not Android.
// TODO(amohammadkhan): Add tests for Cellular/non-cellular connection types
// when support for testing is provided in its class.
TEST(DataUseMeasurementTest, UserNotUserTest) {
  DataUseMeasurementTest data_use_measurement_test;
#if defined(OS_ANDROID)
  data_use_measurement_test.data_use_measurement()
      ->OnApplicationStateChangeForTesting(
          base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
#endif
  data_use_measurement_test.TestForAUserRequest("Foreground.");
}

#if defined(OS_ANDROID)
// This test function tests recording of data use information in UMA histogram
// when packet is originated from user or services when the app is in the
// background and OS is Android.
TEST(DataUseMeasurementTest, ApplicationStateTest) {
  DataUseMeasurementTest data_use_measurement_test;

  data_use_measurement_test.data_use_measurement()
      ->OnApplicationStateChangeForTesting(
          base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  data_use_measurement_test.TestForAUserRequest("Background.");
}
#endif

}  // namespace data_use_measurement
