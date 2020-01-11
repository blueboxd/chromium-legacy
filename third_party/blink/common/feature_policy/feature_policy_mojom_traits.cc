// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/feature_policy/feature_policy_mojom_traits.h"

#include "url/mojom/origin_mojom_traits.h"

namespace mojo {

bool StructTraits<blink::mojom::ParsedFeaturePolicyDeclarationDataView,
                  blink::ParsedFeaturePolicyDeclaration>::
    Read(blink::mojom::ParsedFeaturePolicyDeclarationDataView in,
         blink::ParsedFeaturePolicyDeclaration* out) {
  return in.ReadFeature(&out->feature) &&
         in.ReadFallbackValue(&out->fallback_value) &&
         in.ReadOpaqueValue(&out->opaque_value) && in.ReadValues(&out->values);
}

}  // namespace mojo
