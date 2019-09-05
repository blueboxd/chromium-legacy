// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/vr/vr_eye_parameters.h"

#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

VREyeParameters::VREyeParameters(
    const device::mojom::blink::VREyeParametersPtr& eye_parameters,
    double render_scale) {
  // This only works if the transform only has the translation component.
  // However, WebVR never worked with angled screens anyways.
  const TransformationMatrix matrix(eye_parameters->head_from_eye.matrix());
  offset_ = DOMFloat32Array::Create(3);
  offset_->Data()[0] = matrix.M41();  // x translation
  offset_->Data()[1] = matrix.M42();  // y translation
  offset_->Data()[2] = matrix.M43();  // z translation

  field_of_view_ = MakeGarbageCollected<VRFieldOfView>();
  field_of_view_->SetUpDegrees(eye_parameters->field_of_view->up_degrees);
  field_of_view_->SetDownDegrees(eye_parameters->field_of_view->down_degrees);
  field_of_view_->SetLeftDegrees(eye_parameters->field_of_view->left_degrees);
  field_of_view_->SetRightDegrees(eye_parameters->field_of_view->right_degrees);

  render_width_ = eye_parameters->render_width * render_scale;
  render_height_ = eye_parameters->render_height * render_scale;
}

DOMFloat32Array* VREyeParameters::offset() const {
  return DOMFloat32Array::Create(offset_->Data(), 3);
}

void VREyeParameters::Trace(blink::Visitor* visitor) {
  visitor->Trace(offset_);
  visitor->Trace(field_of_view_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
