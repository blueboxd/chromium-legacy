// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_opener_policy.h"

namespace network {

CrossOriginOpenerPolicy::CrossOriginOpenerPolicy() = default;
CrossOriginOpenerPolicy::CrossOriginOpenerPolicy(
    const CrossOriginOpenerPolicy& src) = default;
CrossOriginOpenerPolicy::CrossOriginOpenerPolicy(
    CrossOriginOpenerPolicy&& src) = default;
CrossOriginOpenerPolicy::~CrossOriginOpenerPolicy() = default;

CrossOriginOpenerPolicy& CrossOriginOpenerPolicy::operator=(
    const CrossOriginOpenerPolicy& src) = default;
CrossOriginOpenerPolicy& CrossOriginOpenerPolicy::operator=(
    CrossOriginOpenerPolicy&& src) = default;
bool CrossOriginOpenerPolicy::operator==(
    const CrossOriginOpenerPolicy& other) const {
  return value == other.value &&
         reporting_endpoint == other.reporting_endpoint &&
         report_only_value == other.report_only_value &&
         report_only_reporting_endpoint == other.report_only_reporting_endpoint;
}

}  // namespace network
