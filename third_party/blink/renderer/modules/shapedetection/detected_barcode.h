// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_DETECTED_BARCODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_DETECTED_BARCODE_H_

#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom-shared.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/imagecapture/point_2d.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DOMRectReadOnly;

class MODULES_EXPORT DetectedBarcode final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DetectedBarcode* Create();
  static DetectedBarcode* Create(String,
                                 DOMRectReadOnly*,
                                 shape_detection::mojom::BarcodeFormat,
                                 HeapVector<Member<Point2D>>);
  static WebString BarcodeFormatToString(
      const shape_detection::mojom::BarcodeFormat format);

  DetectedBarcode(String,
                  DOMRectReadOnly*,
                  shape_detection::mojom::BarcodeFormat,
                  HeapVector<Member<Point2D>>);

  const String& rawValue() const;
  DOMRectReadOnly* boundingBox() const;
  String format() const;
  const HeapVector<Member<Point2D>>& cornerPoints() const;

  ScriptValue toJSONForBinding(ScriptState*) const;
  void Trace(blink::Visitor*) override;

 private:
  const String raw_value_;
  const Member<DOMRectReadOnly> bounding_box_;
  const shape_detection::mojom::BarcodeFormat format_;
  const HeapVector<Member<Point2D>> corner_points_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_DETECTED_BARCODE_H_
