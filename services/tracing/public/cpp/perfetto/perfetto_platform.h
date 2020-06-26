// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_PLATFORM_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_PLATFORM_H_

#include "third_party/perfetto/include/perfetto/tracing/platform.h"

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_local_storage.h"

namespace base {
class DeferredSequencedTaskRunner;
}  // namespace base

namespace tracing {

class PerfettoPlatform : public perfetto::Platform {
 public:
  PerfettoPlatform();
  ~PerfettoPlatform() override;

  void OnThreadPoolAvailable();

  // perfetto::Platform implementation:
  ThreadLocalObject* GetOrCreateThreadLocalObject() override;
  std::unique_ptr<perfetto::base::TaskRunner> CreateTaskRunner(
      const CreateTaskRunnerArgs&) override;
  std::string GetCurrentProcessName() override;

 private:
  scoped_refptr<base::DeferredSequencedTaskRunner> deferred_task_runner_;
  base::ThreadLocalStorage::Slot thread_local_object_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_PLATFORM_H_