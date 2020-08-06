// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_RENDERER_RESOURCE_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_RENDERER_RESOURCE_COORDINATOR_H_

#include "base/macros.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-blink.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ExecutionContext;
class ScriptState;

class PLATFORM_EXPORT RendererResourceCoordinator {
  USING_FAST_MALLOC(RendererResourceCoordinator);

 public:
  // Only initializes if the instrumentation runtime feature is enabled.
  static void MaybeInitialize();
  static RendererResourceCoordinator* Get();

  // Used to switch the current renderer resource coordinator only for testing.
  static void SetCurrentRendererResourceCoordinatorForTesting(
      RendererResourceCoordinator*);

  ~RendererResourceCoordinator();

  void SetMainThreadTaskLoadIsLow(bool);

  // Used for tracking content javascript contexts (frames, workers, worklets,
  // etc). These functions are thread-safe.

  // Called when a |script_state| is created. Note that |execution_context| may
  // be nullptr if the |script_state| is not associated with an
  // |execution_context|.
  void OnScriptStateCreated(ScriptState* script_state,
                            ExecutionContext* execution_context);
  // Called when the |script_state| has been detached from the v8::Context
  // (and ExecutionContext, if applicable) it was associated with at creation.
  // At this point the associated v8::Context is considered "detached" until it
  // is garbage collected.
  void OnScriptStateDetached(ScriptState* script_state);
  // Called when the |script_state| itself is garbage collected.
  void OnScriptStateDestroyed(ScriptState* script_state);

 protected:
  RendererResourceCoordinator();

 private:
  explicit RendererResourceCoordinator(
      mojo::PendingRemote<
          performance_manager::mojom::blink::ProcessCoordinationUnit> remote);

  mojo::Remote<performance_manager::mojom::blink::ProcessCoordinationUnit>
      service_;

  DISALLOW_COPY_AND_ASSIGN(RendererResourceCoordinator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_RENDERER_RESOURCE_COORDINATOR_H_
