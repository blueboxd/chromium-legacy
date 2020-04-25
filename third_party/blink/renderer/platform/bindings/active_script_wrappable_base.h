// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ACTIVE_SCRIPT_WRAPPABLE_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ACTIVE_SCRIPT_WRAPPABLE_BASE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "v8/include/v8.h"

namespace blink {

/**
 * Classes deriving from ActiveScriptWrappable will be kept alive as long as
 * they have a pending activity. Destroying the corresponding ExecutionContext
 * implicitly releases them to avoid leaks.
 */
class PLATFORM_EXPORT ActiveScriptWrappableBase : public GarbageCollectedMixin {
 public:
  virtual ~ActiveScriptWrappableBase() = default;

  virtual bool IsContextDestroyed() const { return false; }
  virtual bool DispatchHasPendingActivity() const { return false; }

 protected:
  // ActiveScriptWrappableBase registers itself with the corresponding
  // ActiveScriptWrappableManager. The default versions of the virtual methods
  // above make sure that in case of a conservative GC, the manager object can
  // already call the virtual methods as non-virtual The objects themselves are
  // generally held alive via conservative stack scan and do not require to be
  // treated as active.
  ActiveScriptWrappableBase();

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveScriptWrappableBase);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ACTIVE_SCRIPT_WRAPPABLE_BASE_H_
