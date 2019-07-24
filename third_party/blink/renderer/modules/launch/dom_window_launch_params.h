// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_DOM_WINDOW_LAUNCH_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_DOM_WINDOW_LAUNCH_PARAMS_H_

#include "third_party/blink/renderer/modules/launch/launch_params.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocalDOMWindow;
class Visitor;

class DOMWindowLaunchParams final
    : public GarbageCollectedFinalized<DOMWindowLaunchParams>,
      public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(DOMWindowLaunchParams);

 public:
  static const char kSupplementName[];

  explicit DOMWindowLaunchParams();
  ~DOMWindowLaunchParams();

  // IDL Interface.
  static Member<LaunchParams> launchParams(LocalDOMWindow&);

  static void UpdateLaunchFiles(LocalDOMWindow*,
                                HeapVector<Member<NativeFileSystemHandle>>);

  void Trace(blink::Visitor*) override;

 private:
  static DOMWindowLaunchParams* FromState(LocalDOMWindow* window);

  Member<LaunchParams> launch_params_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_DOM_WINDOW_LAUNCH_PARAMS_H_
