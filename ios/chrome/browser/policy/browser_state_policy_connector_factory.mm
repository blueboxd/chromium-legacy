// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/browser_state_policy_connector_factory.h"

#include "base/logging.h"
#include "ios/chrome/browser/policy/browser_state_policy_connector.h"
#include "ios/chrome/browser/policy/policy_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

std::unique_ptr<BrowserStatePolicyConnector> BuildBrowserStatePolicyConnector(
    BrowserPolicyConnectorIOS* browser_policy_connector) {
  DCHECK(IsEnterprisePolicyEnabled());

  auto connector = std::make_unique<BrowserStatePolicyConnector>();

  connector->Init(browser_policy_connector);
  return connector;
}
