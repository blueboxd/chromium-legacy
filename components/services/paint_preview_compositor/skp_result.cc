// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/paint_preview_compositor/skp_result.h"

namespace paint_preview {

SkpResult::SkpResult() = default;
SkpResult::~SkpResult() = default;

SkpResult::SkpResult(SkpResult&& other) = default;
SkpResult& SkpResult::operator=(SkpResult&& rhs) = default;

}  // namespace paint_preview
