// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_base.h"

#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

ActiveScriptWrappableBase::ActiveScriptWrappableBase() {
  DCHECK(ThreadState::Current());
  v8::Isolate* isolate = ThreadState::Current()->GetIsolate();
  V8PerIsolateData* isolate_data = V8PerIsolateData::From(isolate);
  isolate_data->GetActiveScriptWrappableManager()->Add(this);
}

}  // namespace blink
