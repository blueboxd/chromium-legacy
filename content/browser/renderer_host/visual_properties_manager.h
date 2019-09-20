// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_VISUAL_PROPERTIES_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_VISUAL_PROPERTIES_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {

class RenderViewHostImpl;
struct VisualProperties;

// This class manages page visual properties in the browser. Visual properties
// can be page-specific or widget-specific. Right now, this state is stored in
// RenderWidgetHost. Eventually, we want page-specific state to be stored in
// this class instead.
class CONTENT_EXPORT VisualPropertiesManager {
 public:
  VisualPropertiesManager(RenderViewHostImpl* render_view_host);
  ~VisualPropertiesManager();

  // Sends both Page and Widget visual properties to the renderer hosting the
  // RenderView. The widget_routing_id is used by the renderer to forward Widget
  // visual properties to the Widget.
  // virtual for testing.
  virtual void SendVisualProperties(const VisualProperties& visual_properties,
                                    int widget_routing_id);

  base::WeakPtr<VisualPropertiesManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  RenderViewHostImpl* render_view_host_ = nullptr;

  // TODO(https://crbug.com/1004009): We should not need a weak factory because
  // RenderWidgetHostImpl should not outlive RenderViewHostImpl.
  // Instances of this class are owned by instances of RenderViewHostImpl. Due
  // to unclear ownership semantics, it's not clear if a RenderWidgetHostImpl
  // can outlive its RenderViewHostImpl. We assume that it can and thus pass a
  // weak ptr.
  base::WeakPtrFactory<VisualPropertiesManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VisualPropertiesManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_VISUAL_PROPERTIES_MANAGER_H_
