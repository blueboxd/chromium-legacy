// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/topic_invalidation_map.h"

#include <stddef.h>

#include "base/values.h"

namespace syncer {

TopicInvalidationMap::TopicInvalidationMap() {}

TopicInvalidationMap::TopicInvalidationMap(const TopicInvalidationMap& other) =
    default;

TopicInvalidationMap::~TopicInvalidationMap() {}

TopicSet TopicInvalidationMap::GetTopics() const {
  TopicSet ret;
  for (const auto& topic_and_invalidation_set : map_)
    ret.insert(topic_and_invalidation_set.first);
  return ret;
}

bool TopicInvalidationMap::Empty() const {
  return map_.empty();
}

void TopicInvalidationMap::Insert(const Invalidation& invalidation) {
  map_[invalidation.topic()].Insert(invalidation);
}

TopicInvalidationMap TopicInvalidationMap::GetSubsetWithTopics(
    const Topics& topics) const {
  std::map<Topic, SingleObjectInvalidationSet> new_map;
  for (const auto& topic : topics) {
    auto lookup = map_.find(topic.first);
    if (lookup != map_.end()) {
      new_map[topic.first] = lookup->second;
    }
  }
  return TopicInvalidationMap(new_map);
}

TopicInvalidationMap TopicInvalidationMap::GetSubsetWithTopics(
    const TopicSet& topics) const {
  std::map<Topic, SingleObjectInvalidationSet> new_map;
  for (const auto& topic : topics) {
    auto lookup = map_.find(topic);
    if (lookup != map_.end()) {
      new_map[topic] = lookup->second;
    }
  }
  return TopicInvalidationMap(new_map);
}

const SingleObjectInvalidationSet& TopicInvalidationMap::ForTopic(
    Topic topic) const {
  auto lookup = map_.find(topic);
  DCHECK(lookup != map_.end());
  DCHECK(!lookup->second.IsEmpty());
  return lookup->second;
}

void TopicInvalidationMap::GetAllInvalidations(
    std::vector<syncer::Invalidation>* out) const {
  for (auto it = map_.begin(); it != map_.end(); ++it) {
    out->insert(out->begin(), it->second.begin(), it->second.end());
  }
}

void TopicInvalidationMap::AcknowledgeAll() const {
  for (auto it1 = map_.begin(); it1 != map_.end(); ++it1) {
    for (auto it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
      it2->Acknowledge();
    }
  }
}

bool TopicInvalidationMap::operator==(const TopicInvalidationMap& other) const {
  return map_ == other.map_;
}

std::unique_ptr<base::ListValue> TopicInvalidationMap::ToValue() const {
  std::unique_ptr<base::ListValue> value(new base::ListValue());
  for (auto it1 = map_.begin(); it1 != map_.end(); ++it1) {
    for (auto it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
      value->Append(it2->ToValue());
    }
  }
  return value;
}

TopicInvalidationMap::TopicInvalidationMap(
    const std::map<Topic, SingleObjectInvalidationSet>& map)
    : map_(map) {}

}  // namespace syncer
