// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"

#include <stddef.h>

#include <utility>

#include "base/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace data_reduction_proxy {

TestDataReductionProxyConfig::TestDataReductionProxyConfig()
    : tick_clock_(nullptr), add_default_proxy_bypass_rules_(true) {}

TestDataReductionProxyConfig::~TestDataReductionProxyConfig() {
}

void TestDataReductionProxyConfig::ResetParamFlagsForTest() {
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

MockDataReductionProxyConfig::MockDataReductionProxyConfig()
    : TestDataReductionProxyConfig() {}

MockDataReductionProxyConfig::~MockDataReductionProxyConfig() {
}

}  // namespace data_reduction_proxy
