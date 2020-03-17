// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_HIGHLIGHT_PATH_GENERATOR_H_
#define UI_VIEWS_CONTROLS_HIGHLIGHT_PATH_GENERATOR_H_

#include <memory>

#include "base/optional.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// HighlightPathGenerators are used to generate its highlight path. This
// highlight path is used to generate the View's focus ring and ink-drop
// effects.
class VIEWS_EXPORT HighlightPathGenerator {
 public:
  struct VIEWS_EXPORT RoundRect {
    // TODO(http://crbug.com/1056490): Remove these constructors and have
    // callsites create a gfx::RoundedCornersF explicitly, or replace this
    // struct with a gfx::RRectF.
    RoundRect();
    RoundRect(const gfx::RectF& bounds, float corner_radius);

    gfx::RectF bounds;
    gfx::RoundedCornersF corner_radius;
  };

  // TODO(http://crbug.com/1056490): Remove this constructor in favor of the one
  // that takes |insets|.
  HighlightPathGenerator();
  explicit HighlightPathGenerator(const gfx::Insets& insets);
  virtual ~HighlightPathGenerator();

  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

  static void Install(View* host,
                      std::unique_ptr<HighlightPathGenerator> generator);
  static base::Optional<RoundRect> GetRoundRectForView(const View* view);

  // TODO(http://crbug.com/1056490): Deprecate |GetHighlightPath()| in favor of
  // |GetRoundRect()|.
  virtual SkPath GetHighlightPath(const View* view);

  // Optionally returns a RoundRect struct which contains data for drawing a
  // highlight. Note that |rect| is in the coordinate system of the view.
  // TODO(http://crbug.com/1056490): Once |GetHighlightPath()| is deprecated,
  // make this a pure virtual function and make the return not optional.
  virtual base::Optional<RoundRect> GetRoundRect(const gfx::RectF& rect);
  base::Optional<RoundRect> GetRoundRect(const View* view);

 private:
  const gfx::Insets insets_;
};

// Sets a highlight path that is empty. This is used for ink drops that want to
// rely on the size of their created ripples/highlights and not have any
// clipping applied to them.
class VIEWS_EXPORT EmptyHighlightPathGenerator : public HighlightPathGenerator {
 public:
  EmptyHighlightPathGenerator() = default;

  EmptyHighlightPathGenerator(const EmptyHighlightPathGenerator&) = delete;
  EmptyHighlightPathGenerator& operator=(const EmptyHighlightPathGenerator&) =
      delete;

  // HighlightPathGenerator:
  base::Optional<RoundRect> GetRoundRect(const gfx::RectF& rect) override;
};

void VIEWS_EXPORT InstallEmptyHighlightPathGenerator(View* view);

// Sets a rectangular highlight path.
class VIEWS_EXPORT RectHighlightPathGenerator : public HighlightPathGenerator {
 public:
  RectHighlightPathGenerator() = default;

  RectHighlightPathGenerator(const RectHighlightPathGenerator&) = delete;
  RectHighlightPathGenerator& operator=(const RectHighlightPathGenerator&) =
      delete;

  // HighlightPathGenerator:
  base::Optional<RoundRect> GetRoundRect(const gfx::RectF& rect) override;
};

void VIEWS_EXPORT InstallRectHighlightPathGenerator(View* view);

// Sets a centered circular highlight path.
class VIEWS_EXPORT CircleHighlightPathGenerator
    : public HighlightPathGenerator {
 public:
  explicit CircleHighlightPathGenerator(const gfx::Insets& insets);

  CircleHighlightPathGenerator(const CircleHighlightPathGenerator&) = delete;
  CircleHighlightPathGenerator& operator=(const CircleHighlightPathGenerator&) =
      delete;

  // HighlightPathGenerator:
  base::Optional<RoundRect> GetRoundRect(const gfx::RectF& rect) override;
};

void VIEWS_EXPORT InstallCircleHighlightPathGenerator(View* view);
void VIEWS_EXPORT
InstallCircleHighlightPathGenerator(View* view, const gfx::Insets& insets);

// Sets a pill-shaped highlight path.
class VIEWS_EXPORT PillHighlightPathGenerator : public HighlightPathGenerator {
 public:
  PillHighlightPathGenerator() = default;

  PillHighlightPathGenerator(const PillHighlightPathGenerator&) = delete;
  PillHighlightPathGenerator& operator=(const PillHighlightPathGenerator&) =
      delete;

  // HighlightPathGenerator:
  SkPath GetHighlightPath(const View* view) override;
};

void VIEWS_EXPORT InstallPillHighlightPathGenerator(View* view);

// TODO(http://crbug.com/1056490): Investigate if we can make |radius| optional
// for FixedSizeCircleHighlightPathGenerator and
// RoundRectHighlightPathGenerator, and combine them with
// CircleHighlightPathGenerator and PillHighlightPathGenerator respectively.
// Sets a centered fixed-size circular highlight path.
class VIEWS_EXPORT FixedSizeCircleHighlightPathGenerator
    : public HighlightPathGenerator {
 public:
  explicit FixedSizeCircleHighlightPathGenerator(int radius);

  FixedSizeCircleHighlightPathGenerator(
      const FixedSizeCircleHighlightPathGenerator&) = delete;
  FixedSizeCircleHighlightPathGenerator& operator=(
      const FixedSizeCircleHighlightPathGenerator&) = delete;

  // HighlightPathGenerator:
  base::Optional<RoundRect> GetRoundRect(const gfx::RectF& rect) override;

 private:
  const int radius_;
};

void VIEWS_EXPORT InstallFixedSizeCircleHighlightPathGenerator(View* view,
                                                               int radius);

// Sets a rounded rectangle highlight path with optional insets.
class VIEWS_EXPORT RoundRectHighlightPathGenerator
    : public HighlightPathGenerator {
 public:
  RoundRectHighlightPathGenerator(const gfx::Insets& insets, int corner_radius);

  RoundRectHighlightPathGenerator(const RoundRectHighlightPathGenerator&) =
      delete;
  RoundRectHighlightPathGenerator& operator=(
      const RoundRectHighlightPathGenerator&) = delete;

  // HighlightPathGenerator:
  base::Optional<RoundRect> GetRoundRect(const gfx::RectF& rect) override;

 private:
  const int corner_radius_;
};

void VIEWS_EXPORT
InstallRoundRectHighlightPathGenerator(View* view,
                                       const gfx::Insets& insets,
                                       int corner_radius);

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_HIGHLIGHT_PATH_GENERATOR_H_
