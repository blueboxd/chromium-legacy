// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_

#include "third_party/blink/public/platform/input/synchronous_layer_tree_frame_sink.h"

namespace blink {

class SynchronousCompositorRegistry {
 public:
  virtual void RegisterLayerTreeFrameSink(
      SynchronousLayerTreeFrameSink* layer_tree_frame_sink) = 0;
  virtual void UnregisterLayerTreeFrameSink(
      SynchronousLayerTreeFrameSink* layer_tree_frame_sink) = 0;

 protected:
  virtual ~SynchronousCompositorRegistry() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_
