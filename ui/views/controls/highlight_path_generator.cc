// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/highlight_path_generator.h"

#include <algorithm>

#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

HighlightPathGenerator::RoundRect::RoundRect() = default;

HighlightPathGenerator::RoundRect::RoundRect(const gfx::RectF& bounds,
                                             float corner_radius)
    : bounds(bounds), corner_radius(corner_radius) {}

HighlightPathGenerator::HighlightPathGenerator()
    : HighlightPathGenerator(gfx::Insets()) {}

HighlightPathGenerator::HighlightPathGenerator(const gfx::Insets& insets)
    : insets_(insets) {}

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
  return SkPath().addRRect(
      SkRRect{gfx::RRectF(round_rect->bounds, round_rect->corner_radius)});
}

base::Optional<HighlightPathGenerator::RoundRect>
HighlightPathGenerator::GetRoundRect(const gfx::RectF& rect) {
  return base::nullopt;
}

base::Optional<HighlightPathGenerator::RoundRect>
HighlightPathGenerator::GetRoundRect(const View* view) {
  gfx::Rect bounds(view->GetLocalBounds());
  bounds.Inset(insets_);
  return GetRoundRect(gfx::RectF(bounds));
}

base::Optional<HighlightPathGenerator::RoundRect>
EmptyHighlightPathGenerator::GetRoundRect(const gfx::RectF& rect) {
  return HighlightPathGenerator::RoundRect();
}

void InstallEmptyHighlightPathGenerator(View* view) {
  HighlightPathGenerator::Install(
      view, std::make_unique<EmptyHighlightPathGenerator>());
}

base::Optional<HighlightPathGenerator::RoundRect>
RectHighlightPathGenerator::GetRoundRect(const gfx::RectF& rect) {
  return HighlightPathGenerator::RoundRect{rect, /*corner_radius=*/0};
}

void InstallRectHighlightPathGenerator(View* view) {
  HighlightPathGenerator::Install(
      view, std::make_unique<RectHighlightPathGenerator>());
}

CircleHighlightPathGenerator::CircleHighlightPathGenerator(
    const gfx::Insets& insets)
    : HighlightPathGenerator(insets) {}

base::Optional<HighlightPathGenerator::RoundRect>
CircleHighlightPathGenerator::GetRoundRect(const gfx::RectF& rect) {
  gfx::RectF bounds = rect;
  const float corner_radius = std::min(bounds.width(), bounds.height()) / 2.f;
  bounds.ClampToCenteredSize(
      gfx::SizeF(corner_radius * 2.f, corner_radius * 2.f));
  return HighlightPathGenerator::RoundRect{bounds, corner_radius};
}

void InstallCircleHighlightPathGenerator(View* view) {
  InstallCircleHighlightPathGenerator(view, gfx::Insets());
}

void InstallCircleHighlightPathGenerator(View* view,
                                         const gfx::Insets& insets) {
  HighlightPathGenerator::Install(
      view, std::make_unique<CircleHighlightPathGenerator>(insets));
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
FixedSizeCircleHighlightPathGenerator::GetRoundRect(const gfx::RectF& rect) {
  gfx::RectF bounds = rect;
  bounds.ClampToCenteredSize(gfx::SizeF(radius_ * 2, radius_ * 2));
  return HighlightPathGenerator::RoundRect{bounds, radius_};
}

void InstallFixedSizeCircleHighlightPathGenerator(View* view, int radius) {
  HighlightPathGenerator::Install(
      view, std::make_unique<FixedSizeCircleHighlightPathGenerator>(radius));
}

RoundRectHighlightPathGenerator::RoundRectHighlightPathGenerator(
    const gfx::Insets& insets,
    int corner_radius)
    : HighlightPathGenerator(insets), corner_radius_(corner_radius) {}

base::Optional<HighlightPathGenerator::RoundRect>
RoundRectHighlightPathGenerator::GetRoundRect(const gfx::RectF& rect) {
  return HighlightPathGenerator::RoundRect{rect, corner_radius_};
}

void InstallRoundRectHighlightPathGenerator(View* view,
                                            const gfx::Insets& insets,
                                            int corner_radius) {
  HighlightPathGenerator::Install(
      view,
      std::make_unique<RoundRectHighlightPathGenerator>(insets, corner_radius));
}

}  // namespace views
