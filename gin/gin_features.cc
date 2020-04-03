// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/gin_features.h"

namespace features {

// Enables optimization of JavaScript in V8.
const base::Feature kV8OptimizeJavascript{"V8OptimizeJavascript",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables flushing of JS bytecode in V8.
const base::Feature kV8FlushBytecode{"V8FlushBytecode",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enables finalizing streaming JS compilations on a background thread.
const base::Feature kV8OffThreadFinalization{"V8OffThreadFinalization",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables lazy feedback allocation in V8.
const base::Feature kV8LazyFeedbackAllocation{"V8LazyFeedbackAllocation",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enable concurrent inlining in TurboFan.
const base::Feature kV8ConcurrentInlining{"V8ConcurrentInlining",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enable per-context marking worklists in V8 GC.
const base::Feature kV8PerContextMarkingWorklist{
    "V8PerContextMarkingWorklist", base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace features
