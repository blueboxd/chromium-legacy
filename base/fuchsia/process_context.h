// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_PROCESS_CONTEXT_H_
#define BASE_FUCHSIA_PROCESS_CONTEXT_H_

#include "base/base_export.h"

namespace sys {
class ComponentInspector;
}  // namespace sys

namespace base {

// Returns sys::ComponentInspector for the current process.
BASE_EXPORT sys::ComponentInspector* ComponentInspectorForProcess();

}  // namespace base

#endif  // BASE_FUCHSIA_PROCESS_CONTEXT_H_
