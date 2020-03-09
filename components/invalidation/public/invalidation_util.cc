// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidation_util.h"

#include <memory>
#include <ostream>
#include <sstream>

#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_handler.h"

namespace syncer {

bool InvalidationVersionLessThan::operator()(const Invalidation& a,
                                             const Invalidation& b) const {
  DCHECK(a.topic() == b.topic()) << "a: " << a.topic() << ", "
                                 << "b: " << b.topic();

  if (a.is_unknown_version() && !b.is_unknown_version())
    return true;

  if (!a.is_unknown_version() && b.is_unknown_version())
    return false;

  if (a.is_unknown_version() && b.is_unknown_version())
    return false;

  return a.version() < b.version();
}

bool operator==(const TopicMetadata& lhs, const TopicMetadata& rhs) {
  return lhs.is_public == rhs.is_public;
}

HandlerOwnerType OwnerNameToHandlerType(const std::string& owner_name) {
  if (owner_name == "Cloud")
    return HandlerOwnerType::kCloud;
  if (owner_name == "Fake")
    return HandlerOwnerType::kFake;
  if (owner_name == "RemoteCommands")
    return HandlerOwnerType::kRemoteCommands;
  if (owner_name == "Drive")
    return HandlerOwnerType::kDrive;
  if (owner_name == "Sync")
    return HandlerOwnerType::kSync;
  if (owner_name == "TICL")
    return HandlerOwnerType::kTicl;
  if (owner_name == "ChildAccountInfoFetcherImpl")
    return HandlerOwnerType::kChildAccount;
  if (owner_name == "NotificationPrinter")
    return HandlerOwnerType::kNotificationPrinter;
  if (owner_name == "InvalidatorShim")
    return HandlerOwnerType::kInvalidatorShim;
  if (owner_name == "SyncEngineImpl")
    return HandlerOwnerType::kSyncEngineImpl;
  return HandlerOwnerType::kUnknown;
}

const Topic* FindMatchingTopic(const Topics& lhs, const Topics& rhs) {
  for (auto lhs_it = lhs.begin(), rhs_it = rhs.begin();
       lhs_it != lhs.end() && rhs_it != rhs.end();) {
    if (lhs_it->first == rhs_it->first) {
      return &lhs_it->first;
    } else if (lhs_it->first < rhs_it->first) {
      ++lhs_it;
    } else {
      ++rhs_it;
    }
  }
  return nullptr;
}

std::vector<Topic> FindRemovedTopics(const Topics& lhs, const Topics& rhs) {
  std::vector<Topic> result;
  for (auto lhs_it = lhs.begin(), rhs_it = rhs.begin(); lhs_it != lhs.end();) {
    if (rhs_it == rhs.end() || lhs_it->first < rhs_it->first) {
      result.push_back(lhs_it->first);
      ++lhs_it;
    } else if (lhs_it->first == rhs_it->first) {
      ++lhs_it;
      ++rhs_it;
    } else {
      ++rhs_it;
    }
  }
  return result;
}

}  // namespace syncer
