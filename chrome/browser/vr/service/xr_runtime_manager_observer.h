// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_XR_RUNTIME_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_VR_SERVICE_XR_RUNTIME_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/vr/vr_export.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"

namespace content {
class BrowserXRRuntime;
}

namespace vr {

class VR_EXPORT XRRuntimeManagerObserver : public base::CheckedObserver {
 public:
  virtual void OnRuntimeAdded(content::BrowserXRRuntime* runtime) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_XR_RUNTIME_MANAGER_OBSERVER_H_
