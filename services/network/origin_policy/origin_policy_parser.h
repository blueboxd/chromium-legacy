// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_PARSER_H_
#define SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_PARSER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "services/network/public/mojom/origin_policy_manager.mojom.h"

namespace base {
class Value;
}  // namespace base

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) OriginPolicyParser {
 public:
  // Parse the given origin policy. Returns an empty policy if parsing is not
  // successful.
  // TODO(vogelheim): Decide how parsing errors should be handled.
  static OriginPolicyContentsPtr Parse(base::StringPiece);

 private:
  OriginPolicyParser();
  ~OriginPolicyParser();

  // The older spec treated parsing errors as failures that would cause an
  // interstitial, so we have a boolean return value to represent that. The
  // newer spec does not treat parsing errors as failures; see
  // https://github.com/WICG/origin-policy/issues/49.
  // TODO(domenic): update everything to the newer spec and remove all boolean
  // return values.

  bool DoParse(base::StringPiece);

  // The following methods are implemented according to the older spec:
  bool ParseContentSecurityPolicies(const base::Value&);
  bool ParseContentSecurityPolicy(const base::Value&);

  // The following method is implemented according to the newer spec:
  void ParseFeatures(const base::Value&);

  OriginPolicyContentsPtr policy_contents_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyParser);
};

}  // namespace network

#endif  // SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_PARSER_H_
