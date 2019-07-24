// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_

#include "base/component_export.h"

namespace tracing {

// If startup tracing command line flags are specified for the process, enables
// TraceLog with config based on the command line flags. Also hooks up service
// callbacks in TraceLog if necessary. The latter is required when the perfetto
// tracing backend is used.
void COMPONENT_EXPORT(TRACING_CPP) EnableStartupTracingIfNeeded();

// Initialize tracing components that require task runners.
void COMPONENT_EXPORT(TRACING_CPP) InitTracingPostThreadPoolStart();

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_
