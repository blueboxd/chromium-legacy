// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_

#include "base/component_export.h"

namespace base {
class CommandLine;
}  // namespace base

namespace tracing {

// Returns true if InitTracingPostThreadPoolStartAndFeatureList has been called
// for this process.
bool COMPONENT_EXPORT(TRACING_CPP) IsTracingInitialized();

// Hooks up hooks up service callbacks in TraceLog for the perfetto backend and,
// if startup tracing command line flags are present, enables TraceLog with a
// config based on the flags. In zygote children, this should only be called
// after mojo is initialized, as the zygote's sandbox prevents creation of the
// tracing SMB before that point.
//
// TODO(eseckler): Consider allocating the SMB in parent processes outside the
// sandbox and supply it via the command line. Then, we can revert to call this
// earlier and from fewer places again.
void COMPONENT_EXPORT(TRACING_CPP) EnableStartupTracingIfNeeded();

// Prepare ProducerClient and trace event and/or sampler profiler data sources
// for startup tracing with chrome's tracing service. Returns false on failure.
// Note that TraceLog still needs to be enabled separately.
//
// TODO(eseckler): Figure out what startup tracing APIs should look like with
// the client lib.
bool COMPONENT_EXPORT(TRACING_CPP)
    SetupStartupTracingForProcess(bool privacy_filtering_enabled,
                                  bool enable_sampler_profiler);

// Initialize tracing components that require task runners. Will switch
// IsTracingInitialized() to return true.
void COMPONENT_EXPORT(TRACING_CPP)
    InitTracingPostThreadPoolStartAndFeatureList();

// If tracing is enabled, grabs the current trace config & mode and tells the
// child to begin tracing right away via startup tracing command line flags.
void COMPONENT_EXPORT(TRACING_CPP)
    PropagateTracingFlagsToChildProcessCmdLine(base::CommandLine* cmd_line);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_
