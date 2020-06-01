// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/swap_result.h"

#include "ui/gfx/gpu_fence.h"

namespace gfx {

SwapCompletionResult::SwapCompletionResult(gfx::SwapResult swap_result)
    : swap_result(swap_result) {}

SwapCompletionResult::SwapCompletionResult(
    gfx::SwapResult swap_result,
    std::unique_ptr<gfx::GpuFence> gpu_fence)
    : swap_result(swap_result), gpu_fence(std::move(gpu_fence)) {}

SwapCompletionResult::SwapCompletionResult(SwapCompletionResult&& other) =
    default;
SwapCompletionResult::~SwapCompletionResult() = default;

}  // namespace gfx
