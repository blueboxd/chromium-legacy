// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/browser_state_policy_connector.h"

#include "components/policy/core/common/policy_service_impl.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserStatePolicyConnector::BrowserStatePolicyConnector() = default;
BrowserStatePolicyConnector::~BrowserStatePolicyConnector() = default;

void BrowserStatePolicyConnector::Init(
    BrowserPolicyConnectorIOS* browser_policy_connector) {
  policy_providers_ = browser_policy_connector->GetPolicyProviders();
  policy_service_ =
      std::make_unique<policy::PolicyServiceImpl>(policy_providers_);
}

void BrowserStatePolicyConnector::Shutdown() {}
