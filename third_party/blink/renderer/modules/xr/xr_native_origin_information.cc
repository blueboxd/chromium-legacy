// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_native_origin_information.h"

#include "third_party/blink/renderer/modules/xr/type_converters.h"
#include "third_party/blink/renderer/modules/xr/xr_anchor.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_plane.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"

namespace blink {

base::Optional<XRNativeOriginInformation> XRNativeOriginInformation::Create(
    const XRAnchor* anchor) {
  DCHECK(anchor);
  return XRNativeOriginInformation(Type::Anchor, anchor->id());
}

base::Optional<XRNativeOriginInformation> XRNativeOriginInformation::Create(
    const XRInputSource* input_source) {
  DCHECK(input_source);
  return XRNativeOriginInformation(Type::InputSource,
                                   input_source->source_id());
}

base::Optional<XRNativeOriginInformation> XRNativeOriginInformation::Create(
    const XRPlane* plane) {
  DCHECK(plane);
  return XRNativeOriginInformation(Type::Plane, plane->id());
}

base::Optional<XRNativeOriginInformation> XRNativeOriginInformation::Create(
    const XRReferenceSpace* reference_space) {
  DCHECK(reference_space);
  // TODO(https://crbug.com/997369): Implement once mojo changes land.
  return base::nullopt;
}

XRNativeOriginInformation::XRNativeOriginInformation(Type type, uint32_t id)
    : id_(id) {}

}  // namespace blink
