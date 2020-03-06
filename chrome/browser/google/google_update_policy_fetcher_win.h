// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_POLICY_FETCHER_WIN_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_POLICY_FETCHER_WIN_H_

#include <memory>

#include "base/values.h"
#include "components/policy/core/browser/policy_conversions.h"

namespace policy {
class PolicyMap;
}

// Returns a list of all the Google Update policies available through the
// IPolicyStatus COM interface.
base::Value GetGoogleUpdatePolicyNames();

// Returns a list of all the Google Update policies available through the
// IPolicyStatus COM interface.
policy::PolicyConversions::PolicyToSchemaMap GetGoogleUpdatePolicySchemas();

// Fetches all the Google Update Policies available through the IPolicyStatus
// COM interface. Only the policies that have been set are returned by this
// function. This function returns null if the fetch fails because IPolicyStatus
// could not be instantiated. This function must run on a COM STA thread because
// it makes some COM calls.
std::unique_ptr<policy::PolicyMap> GetGoogleUpdatePolicies();

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_POLICY_FETCHER_WIN_H_
