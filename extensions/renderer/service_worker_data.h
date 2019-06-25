// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_
#define EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_

#include <memory>

#include "base/macros.h"
#include "extensions/renderer/v8_schema_registry.h"

namespace extensions {
class NativeExtensionBindingsSystem;
class ScriptContext;

// Per ServiceWorker data in worker thread.
// TODO(lazyboy): Also put worker ScriptContexts in this.
class ServiceWorkerData {
 public:
  ServiceWorkerData(
      int64_t service_worker_version_id,
      ScriptContext* context,
      std::unique_ptr<NativeExtensionBindingsSystem> bindings_system);
  ~ServiceWorkerData();

  V8SchemaRegistry* v8_schema_registry() { return v8_schema_registry_.get(); }
  NativeExtensionBindingsSystem* bindings_system() {
    return bindings_system_.get();
  }
  int64_t service_worker_version_id() const {
    return service_worker_version_id_;
  }
  ScriptContext* context() const { return context_; }

  // Returns the number of active interactions for this worker.
  int interaction_count() const { return interaction_count_; }

  // Marks the beginning of an interaction within this worker.
  void IncrementInteraction();
  // Marks the end of an interaction within this worker.
  void DecrementInteraction();

 private:
  const int64_t service_worker_version_id_;
  ScriptContext* const context_;
  int interaction_count_ = 0;

  std::unique_ptr<V8SchemaRegistry> v8_schema_registry_;
  std::unique_ptr<NativeExtensionBindingsSystem> bindings_system_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerData);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_
