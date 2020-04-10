// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_XR_RUNTIME_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_XR_RUNTIME_MANAGER_H_

#include "base/callback_forward.h"
#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"

namespace content {
class BrowserXRRuntime;

// The XRRuntimeManager is a singleton responsible for managing access to the
// active BrowserXRRuntime instances. An Observer interface is provided in case
// runtimes need to interact with runtimes when they are added (e.g. to notify
// them of any current state that they may need to know about).
class XRRuntimeManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnRuntimeAdded(content::BrowserXRRuntime* runtime) {}
  };

  XRRuntimeManager() = default;
  virtual ~XRRuntimeManager() = default;

  XRRuntimeManager(const XRRuntimeManager&) = delete;
  XRRuntimeManager& operator=(const XRRuntimeManager&) = delete;

  // Provides access to the BrowserXRRuntime corresponding to the given
  // XRDeviceId, or nullptr if no such device exists/has been registered.
  virtual content::BrowserXRRuntime* GetRuntime(
      device::mojom::XRDeviceId id) = 0;

  // Provides a mechanism for performing operations on/reasoning about all
  // currently active runtimes, without exposing the collection.
  virtual void ForEachRuntime(
      base::RepeatingCallback<void(content::BrowserXRRuntime*)> fn) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_XR_RUNTIME_MANAGER_H_
