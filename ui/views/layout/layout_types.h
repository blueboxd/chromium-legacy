// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_LAYOUT_TYPES_H_
#define UI_VIEWS_LAYOUT_LAYOUT_TYPES_H_

#include <ostream>
#include <string>
#include <tuple>
#include <utility>

#include "base/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/views_export.h"

namespace views {

// Whether a layout is oriented horizontally or vertically.
enum class LayoutOrientation {
  kHorizontal,
  kVertical,
};

// Stores an optional width and height upper bound. Used when calculating the
// preferred size of a layout pursuant to a maximum available size.
class VIEWS_EXPORT SizeBounds {
 public:
  // Method definitions below to avoid "complex constructor" warning.  Marked
  // explicitly inline because Clang currently doesn't realize that "constexpr"
  // explicitly means "inline" and thus should count as "intentionally inlined
  // and thus shouldn't be warned about".
  // TODO(crbug.com/1045568): Remove "inline" if Clang's isInlineSpecified()
  // learns about constexpr.
  // TODO(crbug.com/1045570): Put method bodies here if complex constructor
  // heuristic learns to peer into types to discover that e.g. Optional is not
  // complex.
  inline constexpr SizeBounds();
  inline constexpr SizeBounds(base::Optional<int> width,
                              base::Optional<int> height);
  inline constexpr explicit SizeBounds(const gfx::Size& size);
  inline constexpr SizeBounds(const SizeBounds&);
  inline constexpr SizeBounds(SizeBounds&&);
  SizeBounds& operator=(const SizeBounds&) = default;
  SizeBounds& operator=(SizeBounds&&) = default;
  ~SizeBounds() = default;

  constexpr const base::Optional<int>& width() const { return width_; }
  void set_width(base::Optional<int> width) { width_ = std::move(width); }

  constexpr const base::Optional<int>& height() const { return height_; }
  void set_height(base::Optional<int> height) { height_ = std::move(height); }

  constexpr bool is_fully_bounded() const { return width_ && height_; }

  // Enlarges (or shrinks, if negative) each upper bound that is present by the
  // specified amounts.
  void Enlarge(int width, int height);

  std::string ToString() const;

 private:
  base::Optional<int> width_;
  base::Optional<int> height_;
};
constexpr SizeBounds::SizeBounds() = default;
constexpr SizeBounds::SizeBounds(base::Optional<int> width,
                                 base::Optional<int> height)
    : width_(std::move(width)), height_(std::move(height)) {}
constexpr SizeBounds::SizeBounds(const gfx::Size& size)
    : width_(size.width()), height_(size.height()) {}
constexpr SizeBounds::SizeBounds(const SizeBounds&) = default;
constexpr SizeBounds::SizeBounds(SizeBounds&&) = default;
constexpr bool operator==(const SizeBounds& lhs, const SizeBounds& rhs) {
  return std::tie(lhs.width(), lhs.height()) ==
         std::tie(rhs.width(), rhs.height());
}
constexpr bool operator!=(const SizeBounds& lhs, const SizeBounds& rhs) {
  return !(lhs == rhs);
}
constexpr bool operator<(const SizeBounds& lhs, const SizeBounds& rhs) {
  return std::tie(lhs.height(), lhs.width()) <
         std::tie(rhs.height(), rhs.width());
}

// Returns true if the specified |size| can fit in the specified |bounds|.
// Returns false if either the width or height of |bounds| is specified and is
// smaller than the corresponding element of |size|.
bool CanFitInBounds(const gfx::Size& size, const SizeBounds& bounds);

// These are declared here for use in gtest-based unit tests but is defined in
// the views_test_support target. Depend on that to use this in your unit test.
// This should not be used in production code - call ToString() instead.
void PrintTo(const SizeBounds& size_bounds, ::std::ostream* os);
void PrintTo(LayoutOrientation layout_orientation, ::std::ostream* os);

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_LAYOUT_TYPES_H_
