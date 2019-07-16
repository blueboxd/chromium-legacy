// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_H_
#define CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "components/policy/core/common/policy_types.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace policy {

// TODO(crbug.com/983174): Refactor the file as these functions have too many
// parameters.

extern const LocalizedString kPolicySources[POLICY_SOURCE_COUNT];

// Returns an array with the values of all set policies, with some values
// converted to be shown in javascript, if it is specified.
// |with_user_policies| governs if values with POLICY_SCOPE_USER are included.
// |convert_types| governs if policy types are returned as string.
base::Value GetAllPolicyValuesAsArray(content::BrowserContext* context,
                                      bool with_user_policies,
                                      bool convert_values,
                                      bool with_device_data,
                                      bool is_pretty_print,
                                      bool convert_types);

// Returns a dictionary with the values of all set policies, with some values
// converted to be shown in javascript, if it is specified.
// |with_user_policies| governs if values with POLICY_SCOPE_USER are included.
// |with_device_data| governs if device identity data (e.g.
// enrollment client ID) and device local accounts policies are included,
// it is used in logs uploads to the server.
// |is_pretty_print| governs if JSON policy value is pretty printed.
// |convert_types| governs if policy types are returned as string.
base::Value GetAllPolicyValuesAsDictionary(content::BrowserContext* context,
                                           bool with_user_policies,
                                           bool convert_values,
                                           bool with_device_data,
                                           bool is_pretty_print,
                                           bool convert_types);

// Returns a JSON with the values of all set policies.
// |with_user_policies| governs if values with POLICY_SCOPE_USER are included.
// |with_device_data| governs if device identity data (e.g.
// enrollment client ID) and device local accounts policies are included,
// it is used in logs uploads to the server.
// |is_pretty_print| governs if the output is formatted.
std::string GetAllPolicyValuesAsJSON(content::BrowserContext* context,
                                     bool with_user_policies,
                                     bool with_device_data,
                                     bool is_pretty_print);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_H_
