// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/render_pass_id_mojom_traits.h"

#include "components/viz/common/quads/render_pass.h"

namespace mojo {

// static
uint64_t StructTraits<viz::mojom::RenderPassIdDataView,
                      viz::RenderPassId>::value(const viz::RenderPassId& id) {
  return static_cast<uint64_t>(id);
}

// static
bool StructTraits<viz::mojom::RenderPassIdDataView, viz::RenderPassId>::Read(
    viz::mojom::RenderPassIdDataView data,
    viz::RenderPassId* out) {
  *out = viz::RenderPassId{data.value()};
  return true;
}

}  // namespace mojo
