// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SANDBOX_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_SANDBOX_TYPE_H_

#include "sandbox/policy/sandbox_type.h"

namespace content {

// TODO(crbug.com/1097376): Remove this header and replace users with
// sandbox/policy/sandbox_type.h.
using SandboxType = sandbox::policy::SandboxType;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SANDBOX_TYPE_H_
