// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_EVENT_CONTEXT_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_EVENT_CONTEXT_H_

#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

// An EventContext allows use of the TRACE_EVENT_BEGIN/TRACE_EVENT_END
// functions to access the protozero TrackEvent to fill in typed arguments.
//
// TODO(nuskos): Switch chrome over to the perfetto client library's
// EventContext.
class EventContext {
 public:
  EventContext(protos::pbzero::TrackEvent* event) : event_(event) {}

  protos::pbzero::TrackEvent* event() const { return event_; }

 private:
  protos::pbzero::TrackEvent* event_;
};

}  // namespace perfetto

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_EVENT_CONTEXT_H_
