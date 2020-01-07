// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBLAYER_BROWSER_INTERFACE_BINDERS_H_
#define WEBLAYER_BROWSER_WEBLAYER_BROWSER_INTERFACE_BINDERS_H_

#include "services/service_manager/public/cpp/binder_map.h"

namespace content {
class RenderFrameHost;
}

namespace weblayer {

void PopulateWebLayerFrameBinders(
    content::RenderFrameHost* render_frame_host,
    service_manager::BinderMapWithContext<content::RenderFrameHost*>*
        binder_map);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBLAYER_BROWSER_INTERFACE_BINDERS_H_
