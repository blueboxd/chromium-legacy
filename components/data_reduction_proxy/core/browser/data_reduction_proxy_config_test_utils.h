// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class TickClock;
}

namespace data_reduction_proxy {

class DataReductionProxyMutableConfigValues;
class TestDataReductionProxyParams;

// Test version of |DataReductionProxyConfig|, which uses an underlying
// |TestDataReductionProxyParams| to permit overriding of default values
// returning from |DataReductionProxyParams|, as well as exposing methods to
// change the underlying state.
class TestDataReductionProxyConfig : public DataReductionProxyConfig {
 public:
  TestDataReductionProxyConfig();

  // Creates a |TestDataReductionProxyConfig| with the provided |config_values|.
  // This permits any DataReductionProxyConfigValues to be used (such as
  // DataReductionProxyParams or DataReductionProxyMutableConfigValues).
  TestDataReductionProxyConfig(
      std::unique_ptr<DataReductionProxyConfigValues> config_values);

  ~TestDataReductionProxyConfig() override;

  // Allows tests to reset the params being used for configuration.
  void ResetParamFlagsForTest();

  // Retrieves the test params being used for the configuration.
  TestDataReductionProxyParams* test_params();

  // Retrieves the underlying config values.
  // TODO(jeremyim): Rationalize with test_params().
  DataReductionProxyConfigValues* config_values();

  // Sets the |tick_clock_| to |tick_clock|. Ownership of |tick_clock| is not
  // passed to the callee.
  void SetTickClock(const base::TickClock* tick_clock);

  base::TimeTicks GetTicksNow() const override;



  void SetShouldAddDefaultProxyBypassRules(bool add_default_proxy_bypass_rules);

  using DataReductionProxyConfig::UpdateConfigForTesting;

 private:

  const base::TickClock* tick_clock_;

  base::Optional<size_t> previous_attempt_counts_;


  // True if the default bypass rules should be added. Should be set to false
  // when fetching resources from an embedded test server running on localhost.
  bool add_default_proxy_bypass_rules_;

  base::Optional<bool> fetch_in_flight_;

  DISALLOW_COPY_AND_ASSIGN(TestDataReductionProxyConfig);
};

// A |TestDataReductionProxyConfig| which permits mocking of methods for
// testing.
class MockDataReductionProxyConfig : public TestDataReductionProxyConfig {
 public:
  // Creates a |MockDataReductionProxyConfig|.
  MockDataReductionProxyConfig(
      std::unique_ptr<DataReductionProxyConfigValues> config_values);
  ~MockDataReductionProxyConfig() override;

  MOCK_CONST_METHOD1(ContainsDataReductionProxy,
                     bool(const net::ProxyConfig::ProxyRules& proxy_rules));
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
