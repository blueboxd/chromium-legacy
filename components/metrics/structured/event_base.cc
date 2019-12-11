// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/event_base.h"

#include "components/metrics/structured/recorder.h"

namespace metrics {
namespace structured {

EventBase::EventBase() = default;
EventBase::EventBase(const EventBase& other) = default;
EventBase::~EventBase() = default;

void EventBase::Record() {
  Recorder::GetInstance()->Record(std::move(*this));
}

EventBase::Metric::Metric(uint64_t name_hash, MetricType type)
    : name_hash(name_hash), type(type) {}
EventBase::Metric::~Metric() = default;

}  // namespace structured
}  // namespace metrics
