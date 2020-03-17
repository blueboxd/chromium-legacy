// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_PARSED_HEADER_H_
#define SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_PARSED_HEADER_H_

#include <string>
#include <vector>
#include "base/component_export.h"
#include "base/optional.h"
#include "services/network/origin_policy/origin_policy_header_values.h"

namespace network {

// Contains the parsed result of an Origin-Policy header, i.e. the output of
// https://wicg.github.io/origin-policy/#parse-an-origin-policy-header.
class COMPONENT_EXPORT(NETWORK_SERVICE) OriginPolicyParsedHeader final {
 public:
  static base::Optional<OriginPolicyParsedHeader> FromString(
      const std::string&);
  ~OriginPolicyParsedHeader();
  OriginPolicyParsedHeader(const OriginPolicyParsedHeader&);

  const std::vector<OriginPolicyAllowedValue>& allowed() const {
    return allowed_;
  }

  const base::Optional<OriginPolicyPreferredValue>& preferred() const {
    return preferred_;
  }

 private:
  OriginPolicyParsedHeader(
      const std::vector<OriginPolicyAllowedValue>& allowed,
      const base::Optional<OriginPolicyPreferredValue>& preferred);

  std::vector<OriginPolicyAllowedValue> allowed_;
  base::Optional<OriginPolicyPreferredValue> preferred_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_PARSED_HEADER_H_
