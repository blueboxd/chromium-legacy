// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_worklet_job.h"

#include "cc/paint/paint_worklet_input.h"

namespace cc {

PaintWorkletJob::PaintWorkletJob(int layer_id,
                                 scoped_refptr<PaintWorkletInput> input)
    : layer_id_(layer_id), input_(std::move(input)) {}

PaintWorkletJob::PaintWorkletJob(const PaintWorkletJob& other) = default;
PaintWorkletJob::PaintWorkletJob(PaintWorkletJob&& other) = default;
PaintWorkletJob::~PaintWorkletJob() = default;

void PaintWorkletJob::SetOutput(sk_sp<PaintRecord> output) {
  DCHECK(!output_);
  output_ = std::move(output);
}

}  // namespace cc
