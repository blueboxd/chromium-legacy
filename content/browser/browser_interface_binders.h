// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_INTERFACE_BINDERS_H_
#define CONTENT_BROWSER_BROWSER_INTERFACE_BINDERS_H_

#include "services/service_manager/public/cpp/binder_map.h"

namespace content {

class RenderFrameHostImpl;
class DedicatedWorkerHost;

namespace internal {

// PopulateBinderMap() registers BrowserInterfaceBroker's GetInterface()
// handler callbacks for different execution context types.
// An implementation of BrowserInterfaceBroker calls the relevant
// PopulateBinderMap() function passing its host execution context instance
// as the first argument and its interface name to handler map as the
// second one.
// This mechanism will replace interface registries and binders used for
// handling InterfaceProvider's GetInterface() calls (see crbug.com/718652).

// Registers the handlers for interfaces requested by frames.
void PopulateBinderMap(RenderFrameHostImpl* rfhi,
                       service_manager::BinderMap* map);

// Registers the handlers for interfaces requested by dedicated workers.
void PopulateBinderMap(DedicatedWorkerHost* dwh,
                       service_manager::BinderMap* map);

}  // namespace internal
}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_INTERFACE_BINDERS_H_
