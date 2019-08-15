// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_WORKLET_INPUT_H_
#define CC_PAINT_PAINT_WORKLET_INPUT_H_

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cc/paint/element_id.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {

class PaintOpBuffer;
using PaintRecord = PaintOpBuffer;

class CC_PAINT_EXPORT PaintWorkletInput
    : public base::RefCountedThreadSafe<PaintWorkletInput> {
 public:
  // Uniquely identifies a property from the animation system, so that a
  // PaintWorkletInput can specify the properties it depends on to be painted
  // (and for which it must be repainted if their values change).
  //
  // PropertyKey is designed to support both native and custom properties. The
  // same ElementId will be produced for all custom properties for a given
  // element. As such we require the custom property name as an additional key
  // to uniquely identify custom properties.
  using PropertyKey = std::pair<std::string, ElementId>;

  virtual gfx::SizeF GetSize() const = 0;

  virtual int WorkletId() const = 0;

  // Return the list of properties used as an input by this PaintWorkletInput.
  // The values for these properties must be provided when dispatching a worklet
  // job for this input.
  using PropertyKeys = std::vector<PropertyKey>;
  virtual const PropertyKeys& GetPropertyKeys() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<PaintWorkletInput>;
  virtual ~PaintWorkletInput() = default;
};

// PaintWorkletRecordMap ties the input for a PaintWorklet (PaintWorkletInput)
// to the painted output (a PaintRecord). It also stores the PaintImage::Id for
// the PaintWorklet to enable efficient invalidation of dirty PaintWorklets.
using PaintWorkletRecordMap =
    base::flat_map<scoped_refptr<const PaintWorkletInput>,
                   std::pair<PaintImage::Id, sk_sp<PaintRecord>>>;

}  // namespace cc

#endif  // CC_PAINT_PAINT_WORKLET_INPUT_H_
