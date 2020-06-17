// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_
#define CONTENT_RENDERER_INPUT_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_

#include "content/renderer/input/synchronous_layer_tree_frame_sink.h"

namespace content {

class SynchronousCompositorRegistry {
 public:
  virtual void RegisterLayerTreeFrameSink(
      SynchronousLayerTreeFrameSink* layer_tree_frame_sink) = 0;
  virtual void UnregisterLayerTreeFrameSink(
      SynchronousLayerTreeFrameSink* layer_tree_frame_sink) = 0;

 protected:
  virtual ~SynchronousCompositorRegistry() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_
