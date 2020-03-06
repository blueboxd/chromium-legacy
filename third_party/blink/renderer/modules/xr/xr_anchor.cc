// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_anchor.h"
#include "third_party/blink/renderer/modules/xr/type_converters.h"
#include "third_party/blink/renderer/modules/xr/xr_object_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"

namespace blink {

XRAnchor::XRAnchor(uint64_t id,
                   XRSession* session,
                   const device::mojom::blink::XRAnchorDataPtr& anchor_data)
    : id_(id),
      session_(session),
      mojo_from_anchor_(std::make_unique<TransformationMatrix>(
          mojo::ConvertTo<blink::TransformationMatrix>(anchor_data->pose))) {}

void XRAnchor::Update(
    const device::mojom::blink::XRAnchorDataPtr& anchor_data) {
  // Just copy, already allocated.
  *mojo_from_anchor_ =
      mojo::ConvertTo<blink::TransformationMatrix>(anchor_data->pose);
}

uint64_t XRAnchor::id() const {
  return id_;
}

XRSpace* XRAnchor::anchorSpace() const {
  DCHECK(mojo_from_anchor_);

  if (!anchor_space_) {
    anchor_space_ =
        MakeGarbageCollected<XRObjectSpace<XRAnchor>>(session_, this);
  }

  return anchor_space_;
}

TransformationMatrix XRAnchor::MojoFromObject() const {
  DCHECK(mojo_from_anchor_);

  return *mojo_from_anchor_;
}

void XRAnchor::detach() {
  session_->xr()->xrEnvironmentProviderRemote()->DetachAnchor(id_);
}

void XRAnchor::Trace(Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(anchor_space_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
