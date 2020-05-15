// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_
#define COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"

// HeapProfilerController controls collection of sampled heap allocation
// snapshots for the current process.
class HeapProfilerController {
 public:
  HeapProfilerController();
  ~HeapProfilerController();

  // Starts periodic heap snapshot collection.
  void Start();

 private:
  using StoppedFlag = base::RefCountedData<base::AtomicFlag>;

  static void ScheduleNextSnapshot(scoped_refptr<StoppedFlag> stopped,
                                   base::TimeDelta heap_collection_interval);
  static void TakeSnapshot(scoped_refptr<StoppedFlag> stopped,
                           base::TimeDelta heap_collection_interval);
  static void RetrieveAndSendSnapshot();

  scoped_refptr<StoppedFlag> stopped_;

  DISALLOW_COPY_AND_ASSIGN(HeapProfilerController);
};

#endif  // COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_
