// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/dom_window_launch_params.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/launch/launch_params.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

const char DOMWindowLaunchParams::kSupplementName[] = "DOMWindowLaunchParams";

DOMWindowLaunchParams::DOMWindowLaunchParams() {}
DOMWindowLaunchParams::~DOMWindowLaunchParams() = default;

Member<LaunchParams> DOMWindowLaunchParams::launchParams(
    LocalDOMWindow& window) {
  return FromState(&window)->launch_params_;
}

void DOMWindowLaunchParams::Trace(blink::Visitor* visitor) {
  visitor->Trace(launch_params_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
DOMWindowLaunchParams* DOMWindowLaunchParams::FromState(
    LocalDOMWindow* window) {
  DOMWindowLaunchParams* supplement =
      Supplement<LocalDOMWindow>::From<DOMWindowLaunchParams>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMWindowLaunchParams>();
    ProvideTo(*window, supplement);

    // TODO(crbug.com/829689): When a service is created for getting launch
    // params from the browser process, use that instead of hard coding the
    // launch params.
    supplement->launch_params_ = MakeGarbageCollected<LaunchParams>(
        HeapVector<Member<NativeFileSystemHandle>>());
  }
  return supplement;
}

}  // namespace blink
