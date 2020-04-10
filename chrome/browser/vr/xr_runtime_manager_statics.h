// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_XR_RUNTIME_MANAGER_STATICS_H_
#define CHROME_BROWSER_VR_XR_RUNTIME_MANAGER_STATICS_H_

#include "content/public/browser/xr_runtime_manager.h"

namespace vr {
// TODO(crbug.com/1031622): Move these methods onto content::XRRuntimeManager.
// Until the whole XRRuntimeManagerImpl is moved, these methods (which modify
// global variables in the file), can't be moved onto XRRuntimeManager as they
// would result in linker errors. They've been moved out here to indicate which
// methods need to move.
class XRRuntimeManagerStatics {
 public:
  // Provides access to the XRRuntimeManager singleton, if it exists.
  // This method does not extend the lifetime of the singleton, so you should be
  // careful with the lifetime of this reference.
  static content::XRRuntimeManager* GetInstanceIfCreated();

  // Exits any currently presenting immersive session.
  static void ExitImmersivePresentation();

  // Observer registration methods are static so that observers may subscribe
  // and unsubscribe independent of the lifecycle of the XRRuntimeManager
  // Singleton.
  static void AddObserver(content::XRRuntimeManager::Observer* observer);
  static void RemoveObserver(content::XRRuntimeManager::Observer* observer);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_XR_RUNTIME_MANAGER_STATICS_H_
