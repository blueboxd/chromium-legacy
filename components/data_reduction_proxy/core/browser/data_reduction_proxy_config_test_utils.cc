// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"

#include <stddef.h>

#include <utility>

#include "base/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace data_reduction_proxy {

TestDataReductionProxyConfig::TestDataReductionProxyConfig()
    : TestDataReductionProxyConfig(
          std::make_unique<TestDataReductionProxyParams>()) {}

TestDataReductionProxyConfig::TestDataReductionProxyConfig(
    std::unique_ptr<DataReductionProxyConfigValues> config_values)
    : DataReductionProxyConfig(
          std::move(config_values)),
      tick_clock_(nullptr),
      add_default_proxy_bypass_rules_(true) {}

TestDataReductionProxyConfig::~TestDataReductionProxyConfig() {
}

void TestDataReductionProxyConfig::ResetParamFlagsForTest() {
  config_values_ = std::make_unique<TestDataReductionProxyParams>();
}

TestDataReductionProxyParams* TestDataReductionProxyConfig::test_params() {
  return static_cast<TestDataReductionProxyParams*>(config_values_.get());
}

DataReductionProxyConfigValues* TestDataReductionProxyConfig::config_values() {
  return config_values_.get();
}

void TestDataReductionProxyConfig::SetTickClock(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

base::TimeTicks TestDataReductionProxyConfig::GetTicksNow() const {
  if (tick_clock_)
    return tick_clock_->NowTicks();
  return DataReductionProxyConfig::GetTicksNow();
}


void TestDataReductionProxyConfig::SetShouldAddDefaultProxyBypassRules(
    bool add_default_proxy_bypass_rules) {
  add_default_proxy_bypass_rules_ = add_default_proxy_bypass_rules;
}

MockDataReductionProxyConfig::MockDataReductionProxyConfig(
    std::unique_ptr<DataReductionProxyConfigValues> config_values)
    : TestDataReductionProxyConfig(std::move(config_values)) {}

MockDataReductionProxyConfig::~MockDataReductionProxyConfig() {
}

}  // namespace data_reduction_proxy
