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
                   const device::mojom::blink::XRAnchorData& anchor_data)
    : id_(id), session_(session) {
  // No need for else - if pose is not present, the default-constructed unique
  // ptr is fine.
  if (anchor_data.pose) {
    SetMojoFromAnchor(
        mojo::ConvertTo<blink::TransformationMatrix>(anchor_data.pose));
  }
}

void XRAnchor::Update(const device::mojom::blink::XRAnchorData& anchor_data) {
  if (anchor_data.pose) {
    SetMojoFromAnchor(
        mojo::ConvertTo<blink::TransformationMatrix>(anchor_data.pose));
  } else {
    mojo_from_anchor_ = nullptr;
  }
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

base::Optional<TransformationMatrix> XRAnchor::MojoFromObject() const {
  if (!mojo_from_anchor_) {
    return base::nullopt;
  }

  return *mojo_from_anchor_;
}

void XRAnchor::detach() {
  session_->xr()->xrEnvironmentProviderRemote()->DetachAnchor(id_);
}

void XRAnchor::SetMojoFromAnchor(const TransformationMatrix& mojo_from_anchor) {
  if (mojo_from_anchor_) {
    *mojo_from_anchor_ = mojo_from_anchor;
  } else {
    mojo_from_anchor_ =
        std::make_unique<TransformationMatrix>(mojo_from_anchor);
  }
}

void XRAnchor::Trace(Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(anchor_space_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
