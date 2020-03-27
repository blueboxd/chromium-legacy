// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_ANCHOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_ANCHOR_H_

#include <memory>

#include "base/optional.h"
#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRSession;
class XRSpace;

class XRAnchor : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRAnchor(uint64_t id,
           XRSession* session,
           const device::mojom::blink::XRAnchorData& anchor_data);

  uint64_t id() const;

  XRSpace* anchorSpace() const;

  base::Optional<TransformationMatrix> MojoFromObject() const;

  void detach();

  void Update(const device::mojom::blink::XRAnchorData& anchor_data);

  void Trace(Visitor* visitor) override;

 private:
  void SetMojoFromAnchor(const TransformationMatrix& mojo_from_anchor);

  const uint64_t id_;

  Member<XRSession> session_;

  // |mojo_from_anchor_| will be non-null in an XRAnchor after the anchor was
  // updated for the first time - this *must* happen in the same frame in which
  // the anchor was created for the anchor to be fully usable. It is currently
  // ensured by XRSession - anchors that got created prior to receiving the
  // result from mojo call to GetFrameData are not returned to the application
  // until their poses are known.
  std::unique_ptr<TransformationMatrix> mojo_from_anchor_;

  // Cached anchor space - it will be created by `anchorSpace()` if it's not
  // set.
  mutable Member<XRSpace> anchor_space_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_ANCHOR_H_
