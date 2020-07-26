// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_share/nearby_share_mojom_traits.h"
#include "base/notreached.h"

namespace mojo {

// static
base::UnguessableToken
StructTraits<nearby_share::mojom::ShareTargetDataView, ShareTarget>::id(
    const ShareTarget& share_target) {
  return share_target.id;
}

// static
std::string StructTraits<nearby_share::mojom::ShareTargetDataView,
                         ShareTarget>::name(const ShareTarget& share_target) {
  return share_target.device_name;
}

// static
nearby_share::mojom::ShareTargetType
StructTraits<nearby_share::mojom::ShareTargetDataView, ShareTarget>::type(
    const ShareTarget& share_target) {
  return share_target.type;
}

// static
bool StructTraits<nearby_share::mojom::ShareTargetDataView, ShareTarget>::Read(
    nearby_share::mojom::ShareTargetDataView data,
    ShareTarget* out) {
  return data.ReadId(&out->id) && data.ReadName(&out->device_name) &&
         data.ReadType(&out->type);
}

}  // namespace mojo
