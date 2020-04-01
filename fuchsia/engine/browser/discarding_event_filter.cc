// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/discarding_event_filter.h"

DiscardingEventFilter::DiscardingEventFilter() = default;

DiscardingEventFilter::~DiscardingEventFilter() = default;

ui::EventDispatchDetails DiscardingEventFilter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (discard_events_)
    return DiscardEvent(continuation);
  return SendEvent(continuation, &event);
}
