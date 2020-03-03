// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_INVALIDATION_UTIL_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_INVALIDATION_UTIL_H_

#include "components/invalidation/public/invalidation_util.h"

namespace enterprise_management {

class PolicyData;

}  // namespace enterprise_management

namespace policy {

// Returns true if |topic| is a public topic. Topic can be either public or
// private. Private topic is keyed by GAIA ID, while public isn't, so many
// users can be subscribed to the same public topic.
// For example:
// If client subscribes to "DeviceGuestModeEnabled" public topic all the
// clients subscribed to this topic will receive all the outgoing messages
// addressed to topic "DeviceGuestModeEnabled". But if 2 clients with different
// users subscribe to private topic "BOOKMARK", they will receive different set
// of messages addressed to pair ("BOOKMARK", GAIA ID) respectievely.
bool IsPublicInvalidationTopic(const syncer::Topic& topic);

// Returns true if |policy| has data about policy to invalidate, and saves
// that data in |topic|, and false otherwise.
bool GetCloudPolicyTopicFromPolicy(
    const enterprise_management::PolicyData& policy,
    syncer::Topic* topic);

// The same as GetCloudPolicyTopicFromPolicy but gets the |topic| for
// remote command.
bool GetRemoteCommandTopicFromPolicy(
    const enterprise_management::PolicyData& policy,
    syncer::Topic* topic);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_INVALIDATION_UTIL_H_
