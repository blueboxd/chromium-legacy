// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_WORKLET_JOB_H_
#define CC_PAINT_PAINT_WORKLET_JOB_H_

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {

class PaintWorkletInput;

// A PaintWorkletJob instance encapsulates the data that needs to be passed
// around in order to dispatch PaintWorklets to the worklet thread, paint them,
// and return the results to cc-impl.
class CC_PAINT_EXPORT PaintWorkletJob {
 public:
  PaintWorkletJob(int layer_id, scoped_refptr<PaintWorkletInput> input);
  PaintWorkletJob(const PaintWorkletJob& other);
  PaintWorkletJob(PaintWorkletJob&& other);
  ~PaintWorkletJob();

  int layer_id() const { return layer_id_; }
  const scoped_refptr<PaintWorkletInput>& input() const { return input_; }
  const sk_sp<PaintRecord>& output() const { return output_; }

  void SetOutput(sk_sp<PaintRecord> output);

 private:
  // The id for the layer that the PaintWorkletInput is associated with.
  int layer_id_;

  // The input for a PaintWorkletJob is encapsulated in a PaintWorkletInput
  // instance; see class-level comments on |PaintWorkletInput| for details.
  scoped_refptr<PaintWorkletInput> input_;

  // The output for a PaintWorkletJob is a series of paint ops for the painted
  // content, that can be passed to raster.
  sk_sp<PaintRecord> output_;
};

// The PaintWorklet dispatcher logic passes the PaintWorkletJobVector to the
// worklet thread during painting. To keep the structure alive on both the
// compositor and worklet side (as technically the compositor could be town down
// whilst the worklet is still painting), we use base::RefCountedJob for it.
using PaintWorkletJobVector =
    base::RefCountedData<std::vector<PaintWorkletJob>>;
using PaintWorkletId = int;
using PaintWorkletJobMap =
    base::flat_map<PaintWorkletId, scoped_refptr<PaintWorkletJobVector>>;

}  // namespace cc

#endif  // CC_PAINT_PAINT_WORKLET_JOB_H_
