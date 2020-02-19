// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xmlhttprequest/main_thread_disallow_synchronous_xhr_scope.h"

#include "base/logging.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

static unsigned g_disallow_synchronous_xhr_count = 0;

MainThreadDisallowSynchronousXHRScope::MainThreadDisallowSynchronousXHRScope() {
  DCHECK(IsMainThread());
  ++g_disallow_synchronous_xhr_count;
}

MainThreadDisallowSynchronousXHRScope::
    ~MainThreadDisallowSynchronousXHRScope() {
  DCHECK(IsMainThread());
  --g_disallow_synchronous_xhr_count;
}

bool MainThreadDisallowSynchronousXHRScope::ShouldDisallowSynchronousXHR() {
  DCHECK(IsMainThread());
  return g_disallow_synchronous_xhr_count > 0;
}

}  // namespace blink
