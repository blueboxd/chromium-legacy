// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/process_context.h"

#include <lib/sys/inspect/cpp/component.h>

#include "base/fuchsia/default_context.h"
#include "base/no_destructor.h"

namespace base {

sys::ComponentInspector* ComponentInspectorForProcess() {
  static base::NoDestructor<sys::ComponentInspector> value(
      fuchsia::ComponentContextForCurrentProcess());
  return value.get();
}

}  // namespace base
