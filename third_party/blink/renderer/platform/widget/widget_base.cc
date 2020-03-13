// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/widget_base.h"

#include "third_party/blink/renderer/platform/widget/widget_base_client.h"

namespace blink {

WidgetBase::WidgetBase(WidgetBaseClient* client) : client_(client) {}

WidgetBase::~WidgetBase() = default;

void WidgetBase::SetCompositorHosts(cc::LayerTreeHost* layer_tree_host,
                                    cc::AnimationHost* animation_host) {
  layer_tree_host_ = layer_tree_host;
  animation_host_ = animation_host;
}

cc::LayerTreeHost* WidgetBase::LayerTreeHost() const {
  return layer_tree_host_;
}

cc::AnimationHost* WidgetBase::AnimationHost() const {
  return animation_host_;
}

void WidgetBase::BeginMainFrame(base::TimeTicks frame_time) {
  client_->DispatchRafAlignedInput(frame_time);
  client_->BeginMainFrame(frame_time);
}

}  // namespace blink
