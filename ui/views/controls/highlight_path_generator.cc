// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/highlight_path_generator.h"

#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

HighlightPathGenerator::~HighlightPathGenerator() = default;

// static
void HighlightPathGenerator::Install(
    View* host,
    std::unique_ptr<HighlightPathGenerator> generator) {
  host->SetProperty(kHighlightPathGeneratorKey, generator.release());
}

// static
base::Optional<HighlightPathGenerator::RoundRect>
HighlightPathGenerator::GetRoundRectForView(const View* view) {
  HighlightPathGenerator* path_generator =
      view->GetProperty(kHighlightPathGeneratorKey);
  return path_generator ? path_generator->GetRoundRect(view) : base::nullopt;
}

SkPath HighlightPathGenerator::GetHighlightPath(const View* view) {
  // A rounded rectangle must be supplied if using this default implementation.
  base::Optional<HighlightPathGenerator::RoundRect> round_rect =
      GetRoundRect(view);
  DCHECK(round_rect);

  const float corner_radius = round_rect->corner_radius;
  return SkPath().addRoundRect(
      gfx::RectToSkRect(gfx::ToNearestRect(round_rect->bounds)), corner_radius,
      corner_radius);
}

base::Optional<HighlightPathGenerator::RoundRect>
HighlightPathGenerator::GetRoundRect(const View* view) {
  return base::nullopt;
}

SkPath RectHighlightPathGenerator::GetHighlightPath(const View* view) {
  return SkPath().addRect(gfx::RectToSkRect(view->GetLocalBounds()));
}

void InstallRectHighlightPathGenerator(View* view) {
  HighlightPathGenerator::Install(
      view, std::make_unique<RectHighlightPathGenerator>());
}

base::Optional<HighlightPathGenerator::RoundRect>
CircleHighlightPathGenerator::GetRoundRect(const View* view) {
  gfx::RectF bounds(view->GetLocalBounds());
  const float corner_radius = std::min(bounds.width(), bounds.height()) / 2.f;
  bounds.ClampToCenteredSize(
      gfx::SizeF(corner_radius * 2.f, corner_radius * 2.f));
  HighlightPathGenerator::RoundRect round_rect;
  round_rect.bounds = bounds;
  round_rect.corner_radius = corner_radius;
  return base::make_optional(round_rect);
}

void InstallCircleHighlightPathGenerator(View* view) {
  HighlightPathGenerator::Install(
      view, std::make_unique<CircleHighlightPathGenerator>());
}

SkPath PillHighlightPathGenerator::GetHighlightPath(const View* view) {
  const SkRect rect = gfx::RectToSkRect(view->GetLocalBounds());
  const SkScalar corner_radius =
      SkScalarHalf(std::min(rect.width(), rect.height()));

  return SkPath().addRoundRect(gfx::RectToSkRect(view->GetLocalBounds()),
                               corner_radius, corner_radius);
}

void InstallPillHighlightPathGenerator(View* view) {
  HighlightPathGenerator::Install(
      view, std::make_unique<PillHighlightPathGenerator>());
}

FixedSizeCircleHighlightPathGenerator::FixedSizeCircleHighlightPathGenerator(
    int radius)
    : radius_(radius) {}

base::Optional<HighlightPathGenerator::RoundRect>
FixedSizeCircleHighlightPathGenerator::GetRoundRect(const View* view) {
  gfx::RectF bounds(view->GetLocalBounds());
  bounds.ClampToCenteredSize(gfx::SizeF(radius_ * 2, radius_ * 2));
  HighlightPathGenerator::RoundRect round_rect;
  round_rect.bounds = bounds;
  round_rect.corner_radius = radius_;
  return base::make_optional(round_rect);
}

void InstallFixedSizeCircleHighlightPathGenerator(View* view, int radius) {
  HighlightPathGenerator::Install(
      view, std::make_unique<FixedSizeCircleHighlightPathGenerator>(radius));
}

}  // namespace views
