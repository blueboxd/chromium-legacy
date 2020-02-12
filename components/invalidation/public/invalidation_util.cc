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
#include "google/cacheinvalidation/include/types.h"

namespace syncer {

const int kDeprecatedSourceForFCM = 2000;

bool ObjectIdLessThan::operator()(const invalidation::ObjectId& lhs,
                                  const invalidation::ObjectId& rhs) const {
  return (lhs.source() < rhs.source()) ||
         (lhs.source() == rhs.source() && lhs.name() < rhs.name());
}

bool InvalidationVersionLessThan::operator()(const Invalidation& a,
                                             const Invalidation& b) const {
  DCHECK(a.object_id() == b.object_id())
      << "a: " << ObjectIdToString(a.object_id()) << ", "
      << "b: " << ObjectIdToString(a.object_id());

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

std::unique_ptr<base::DictionaryValue> ObjectIdToValue(
    const invalidation::ObjectId& object_id) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetInteger("source", object_id.source());
  value->SetString("name", object_id.name());
  return value;
}

bool ObjectIdFromValue(const base::DictionaryValue& value,
                       invalidation::ObjectId* out) {
  *out = invalidation::ObjectId();
  std::string name;
  int source = 0;
  if (!value.GetInteger("source", &source) || !value.GetString("name", &name)) {
    return false;
  }
  *out = invalidation::ObjectId(source, name);
  return true;
}

std::string ObjectIdToString(const invalidation::ObjectId& object_id) {
  std::string str;
  base::JSONWriter::Write(*ObjectIdToValue(object_id), &str);
  return str;
}

Topics ConvertIdsToTopics(ObjectIdSet ids, InvalidationHandler* handler) {
  Topics topics;
  for (const auto& id : ids)
    topics.emplace(id.name(), TopicMetadata{handler->IsPublicTopic(id.name())});
  return topics;
}

ObjectIdSet ConvertTopicsToIds(TopicSet topics) {
  ObjectIdSet ids;
  for (const auto& topic : topics)
    ids.insert(invalidation::ObjectId(kDeprecatedSourceForFCM, topic));
  return ids;
}

ObjectIdSet ConvertTopicsToIds(Topics topics) {
  ObjectIdSet ids;
  for (const auto& topic : topics)
    ids.insert(invalidation::ObjectId(kDeprecatedSourceForFCM, topic.first));
  return ids;
}

invalidation::ObjectId ConvertTopicToId(const Topic& topic) {
  return invalidation::ObjectId(kDeprecatedSourceForFCM, topic);
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
