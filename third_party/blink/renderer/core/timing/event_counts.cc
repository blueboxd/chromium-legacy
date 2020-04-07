// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/event_counts.h"

namespace blink {

class EventCountsIterationSource final
    : public PairIterable<AtomicString, unsigned>::IterationSource {
 public:
  explicit EventCountsIterationSource(const EventCounts& map)
      : map_(map), iterator_(map_->Map().begin()) {}

  bool Next(ScriptState* script_state,
            AtomicString& map_key,
            unsigned& map_value,
            ExceptionState&) override {
    if (iterator_ == map_->Map().end())
      return false;
    map_key = iterator_->key;
    map_value = iterator_->value;
    ++iterator_;
    return true;
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(map_);
    PairIterable<AtomicString, unsigned>::IterationSource::Trace(visitor);
  }

 private:
  // Needs to be kept alive while we're iterating over it.
  const Member<const EventCounts> map_;
  HashMap<AtomicString, unsigned>::const_iterator iterator_;
};

void EventCounts::Add(const AtomicString& event_type) {
  auto iterator = event_count_map_.find(event_type);
  if (iterator == event_count_map_.end()) {
    event_count_map_.insert(event_type, 1u);
  } else {
    iterator->value++;
  }
}

PairIterable<AtomicString, unsigned>::IterationSource*
EventCounts::StartIteration(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<EventCountsIterationSource>(*this);
}

bool EventCounts::GetMapEntry(ScriptState*,
                              const AtomicString& key,
                              unsigned& value,
                              ExceptionState&) {
  auto it = event_count_map_.find(key);
  if (it == event_count_map_.end())
    return false;

  value = it->value;
  return true;
}

}  // namespace blink
