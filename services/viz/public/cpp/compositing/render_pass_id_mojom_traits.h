// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RENDER_PASS_ID_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RENDER_PASS_ID_MOJOM_TRAITS_H_

#include "components/viz/common/quads/render_pass.h"
#include "services/viz/public/mojom/compositing/render_pass_id.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::RenderPassIdDataView, viz::RenderPassId> {
  static uint64_t value(const viz::RenderPassId& id);

  static bool Read(viz::mojom::RenderPassIdDataView data,
                   viz::RenderPassId* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RENDER_PASS_ID_MOJOM_TRAITS_H_
