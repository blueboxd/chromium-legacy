// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_WORKLET_INPUT_H_
#define CC_PAINT_PAINT_WORKLET_INPUT_H_

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {

class PaintOpBuffer;
using PaintRecord = PaintOpBuffer;

class CC_PAINT_EXPORT PaintWorkletInput
    : public base::RefCountedThreadSafe<PaintWorkletInput> {
 public:
  virtual gfx::SizeF GetSize() const = 0;
  virtual int WorkletId() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<PaintWorkletInput>;
  virtual ~PaintWorkletInput() = default;
};

// PaintWorkletRecordMap ties the input for a PaintWorklet (PaintWorkletInput)
// to the painted output (a PaintRecord).
using PaintWorkletRecordMap =
    base::flat_map<scoped_refptr<PaintWorkletInput>, sk_sp<PaintRecord>>;

}  // namespace cc

#endif  // CC_PAINT_PAINT_WORKLET_INPUT_H_
