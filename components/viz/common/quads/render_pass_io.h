// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_IO_H_
#define COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_IO_H_

#include <memory>

#include "base/values.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {
VIZ_COMMON_EXPORT base::Value RenderPassToDict(const RenderPass& render_pass);
VIZ_COMMON_EXPORT std::unique_ptr<RenderPass> RenderPassFromDict(
    const base::Value& dict);

VIZ_COMMON_EXPORT base::Value RenderPassListToDict(
    const RenderPassList& render_pass_list);
VIZ_COMMON_EXPORT bool RenderPassListFromDict(const base::Value& dict,
                                              RenderPassList* render_pass_list);
}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_IO_H_
