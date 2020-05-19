// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SERVICE_SANDBOX_TYPE_H_
#define CHROME_BROWSER_CHROMEOS_SERVICE_SANDBOX_TYPE_H_

#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/sandbox_type.h"
#include "content/public/browser/service_process_host.h"

// This file maps service classes to sandbox types.  Services which
// require a non-utility sandbox can be added here.  See
// ServiceProcessHost::Launch() for how these templates are consumed.

// chromeos::ime::mojom::InputEngineManager
namespace chromeos {
namespace ime {
namespace mojom {
class InputEngineManager;
}  // namespace mojom
}  // namespace ime
}  // namespace chromeos

template <>
content::SandboxType
content::GetServiceSandboxType<chromeos::ime::mojom::InputEngineManager>() {
  if (chromeos::features::IsImeSandboxEnabled())
    return content::SandboxType::kIme;

  return content::SandboxType::kUtility;
}

#endif  // CHROME_BROWSER_CHROMEOS_SERVICE_SANDBOX_TYPE_H_
