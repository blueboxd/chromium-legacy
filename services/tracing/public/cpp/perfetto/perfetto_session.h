// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_SESSION_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_SESSION_H_

#include <set>
#include <string>

#include "base/component_export.h"

namespace perfetto {
namespace protos {
namespace gen {
class TraceStats;
}  // namespace gen
}  // namespace protos
}  // namespace perfetto

namespace tracing {

// Helpers for deriving information from Perfetto's tracing session statistics.
double COMPONENT_EXPORT(TRACING_CPP)
    GetTraceBufferUsage(const perfetto::protos::gen::TraceStats&);
bool COMPONENT_EXPORT(TRACING_CPP)
    HasLostData(const perfetto::protos::gen::TraceStats&);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_SESSION_H_