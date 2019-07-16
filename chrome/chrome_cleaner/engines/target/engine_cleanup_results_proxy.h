// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_CLEANUP_RESULTS_PROXY_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_CLEANUP_RESULTS_PROXY_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "chrome/chrome_cleaner/mojom/engine_sandbox.mojom.h"

namespace chrome_cleaner {

// Accessors to send the cleanup results over the Mojo connection.
class EngineCleanupResultsProxy
    : public base::RefCountedThreadSafe<EngineCleanupResultsProxy> {
 public:
  EngineCleanupResultsProxy(
      mojom::EngineCleanupResultsAssociatedPtr cleanup_results_ptr,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  void UnbindCleanupResultsPtr();

  // Sends a cleanup done signal to the broken process. Will be called on an
  // arbitrary thread from the sandboxed engine.
  void CleanupDone(uint32_t result);

 private:
  friend class base::RefCountedThreadSafe<EngineCleanupResultsProxy>;
  ~EngineCleanupResultsProxy();

  // Invokes cleanup_results_ptr_->Done from the IPC thread.
  void OnDone(uint32_t result);

  // An EngineCleanupResults that will send the results over the Mojo
  // connection.
  mojom::EngineCleanupResultsAssociatedPtr cleanup_results_ptr_;

  // A task runner for the IPC thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_CLEANUP_RESULTS_PROXY_H_
