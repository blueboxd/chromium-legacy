// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_mojom_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "ui/gfx/image/mojom/image_skia_mojom_traits.h"
#include "ui/gfx/range/mojom/range_mojom_traits.h"

namespace mojo {

////////////////////////////////////////////////////////////////////////////////
// SearchResultTag:

// static
bool StructTraits<ash::mojom::SearchResultTagDataView, ash::SearchResultTag>::
    Read(ash::mojom::SearchResultTagDataView data, ash::SearchResultTag* out) {
  if (!data.ReadRange(&out->range))
    return false;
  out->styles = data.styles();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// SearchResultAction:

// static
bool StructTraits<
    ash::mojom::SearchResultActionDataView,
    ash::SearchResultAction>::Read(ash::mojom::SearchResultActionDataView data,
                                   ash::SearchResultAction* out) {
  if (!data.ReadTooltipText(&out->tooltip_text))
    return false;
  if (!data.ReadImage(&out->image))
    return false;
  out->visible_on_hover = data.visible_on_hover();
  return true;
}

}  // namespace mojo
