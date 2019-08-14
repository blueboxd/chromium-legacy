// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_

#include "base/component_export.h"

namespace tracing {

// Returns true if InitTracingPostThreadPoolStartAndFeatureList has been called
// for this process.
bool COMPONENT_EXPORT(TRACING_CPP) IsTracingInitialized();

// TraceLog with config based on the command line flags. Also hooks up service
// callbacks in TraceLog if necessary. The latter is required when the perfetto
// tracing backend is used.
void COMPONENT_EXPORT(TRACING_CPP) EnableStartupTracingIfNeeded();

// Initialize tracing components that require task runners. Will switch
// IsTracingInitialized() to return true.
void COMPONENT_EXPORT(TRACING_CPP)
    InitTracingPostThreadPoolStartAndFeatureList();

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_
