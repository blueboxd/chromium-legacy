// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_INTERFACE_BINDERS_H_
#define CONTENT_BROWSER_BROWSER_INTERFACE_BINDERS_H_

#include "services/service_manager/public/cpp/binder_map.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;
class DedicatedWorkerHost;
class SharedWorkerHost;

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
void PopulateBinderMapWithContext(
    RenderFrameHostImpl* rfhi,
    service_manager::BinderMapWithContext<RenderFrameHost*>* map);
RenderFrameHost* GetContextForHost(RenderFrameHostImpl* rfhi);

// Registers the handlers for interfaces requested by dedicated workers.
void PopulateBinderMap(DedicatedWorkerHost* dwh,
                       service_manager::BinderMap* map);
void PopulateBinderMapWithContext(
    DedicatedWorkerHost* dwh,
    service_manager::BinderMapWithContext<const url::Origin&>* map);
const url::Origin& GetContextForHost(DedicatedWorkerHost* dwh);

// Registers the handlers for interfaces requested by shared workers.
void PopulateBinderMap(SharedWorkerHost* swh, service_manager::BinderMap* map);
void PopulateBinderMapWithContext(
    SharedWorkerHost* swh,
    service_manager::BinderMapWithContext<const url::Origin&>* map);
url::Origin GetContextForHost(SharedWorkerHost* swh);

}  // namespace internal
}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_INTERFACE_BINDERS_H_
