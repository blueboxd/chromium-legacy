// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/ca_layer_overlay.h"

#include "gpu/GLES2/gl2extchromium.h"

namespace viz {

CALayerOverlay::CALayerOverlay() : filter(GL_LINEAR) {}

CALayerOverlay::CALayerOverlay(const CALayerOverlay& other) = default;

CALayerOverlay::~CALayerOverlay() {}

}  // namespace viz
