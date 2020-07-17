// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_SERVICE_SANDBOX_TYPE_H_
#define COMPONENTS_PRINTING_BROWSER_SERVICE_SANDBOX_TYPE_H_

#include "content/public/browser/service_process_host.h"
#include "sandbox/policy/sandbox_type.h"

// This file maps service classes to sandbox types.  Services which
// require a non-utility sandbox can be added here.  See
// ServiceProcessHost::Launch() for how these templates are consumed.

// printing::mojom::PrintCompositor
namespace printing {
namespace mojom {
class PrintCompositor;
}
}  // namespace printing

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<printing::mojom::PrintCompositor>() {
  return sandbox::policy::SandboxType::kPrintCompositor;
}

#endif  // COMPONENTS_PRINTING_BROWSER_SERVICE_SANDBOX_TYPE_H_
