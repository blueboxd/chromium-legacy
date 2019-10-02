// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/highlight_path_generator.h"

#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

HighlightPathGenerator::~HighlightPathGenerator() = default;

void HighlightPathGenerator::Install(
    View* host,
    std::unique_ptr<HighlightPathGenerator> generator) {
  host->SetProperty(kHighlightPathGeneratorKey, generator.release());
}

SkPath RectHighlightPathGenerator::GetHighlightPath(const View* view) {
  SkPath path;
  path.addRect(gfx::RectToSkRect(view->GetLocalBounds()));
  return path;
}

void InstallRectHighlightPathGenerator(View* view) {
  HighlightPathGenerator::Install(
      view, std::make_unique<RectHighlightPathGenerator>());
}

}  // namespace views
