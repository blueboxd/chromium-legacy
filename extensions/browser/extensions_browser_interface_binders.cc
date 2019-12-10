// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_browser_interface_binders.h"

#include "base/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/mojo/keep_alive_impl.h"
#include "extensions/common/mojom/keep_alive.mojom.h"  // nogncheck

namespace extensions {

void PopulateExtensionFrameBinders(service_manager::BinderMapWithContext<
                                       content::RenderFrameHost*>* binder_map,
                                   content::RenderFrameHost* render_frame_host,
                                   const Extension* extension) {
  DCHECK(extension);

  binder_map->Add<KeepAlive>(
      base::BindRepeating(&KeepAliveImpl::Create,
                          render_frame_host->GetProcess()->GetBrowserContext(),
                          base::RetainedRef(extension)));
}

}  // namespace extensions
