// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/policy_invalidation_util.h"

#include "base/strings/string_piece.h"
#include "components/invalidation/public/invalidation.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google/cacheinvalidation/include/types.h"

namespace policy {

namespace {

constexpr char kFcmPolicyPublicTopicPrefix[] = "cs-";

}  // namespace

bool IsPublicInvalidationTopic(const syncer::Topic& topic) {
  return base::StringPiece(topic).starts_with(kFcmPolicyPublicTopicPrefix);
}

bool GetCloudPolicyObjectIdFromPolicy(
    const enterprise_management::PolicyData& policy,
    invalidation::ObjectId* object_id) {
  if (!policy.has_policy_invalidation_topic() ||
      policy.policy_invalidation_topic().empty()) {
    return false;
  }
  *object_id = invalidation::ObjectId(syncer::kDeprecatedSourceForFCM,
                                      policy.policy_invalidation_topic());
  return true;
}

bool GetRemoteCommandObjectIdFromPolicy(
    const enterprise_management::PolicyData& policy,
    invalidation::ObjectId* object_id) {
  if (!policy.has_command_invalidation_topic() ||
      policy.command_invalidation_topic().empty()) {
    return false;
  }
  *object_id = invalidation::ObjectId(syncer::kDeprecatedSourceForFCM,
                                      policy.command_invalidation_topic());
  return true;
}

}  // namespace policy
