// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/interpolating_layout_manager.h"

#include <memory>
#include <utility>

#include "ui/views/view.h"

namespace views {

namespace {

using ChildLayout = LayoutManagerBase::ChildLayout;
using ProposedLayout = LayoutManagerBase::ProposedLayout;

int InterpolateValue(int first, int second) {
  return (first + second) / 2;
}

gfx::Size InterpolateSize(const gfx::Size& first, const gfx::Size& second) {
  return gfx::Size(InterpolateValue(first.width(), second.width()),
                   InterpolateValue(first.height(), second.height()));
}

gfx::Rect InterpolateRect(const gfx::Rect& first, const gfx::Rect& second) {
  return gfx::Rect(InterpolateValue(first.x(), second.x()),
                   InterpolateValue(first.y(), second.y()),
                   InterpolateValue(first.width(), second.width()),
                   InterpolateValue(first.height(), second.height()));
}

// static
ProposedLayout Interpolate(const ProposedLayout& left,
                           const ProposedLayout& right) {
  ProposedLayout layout;
  const size_t num_children = left.child_layouts.size();
  DCHECK_EQ(num_children, right.child_layouts.size());
  layout.host_size = InterpolateSize(left.host_size, right.host_size);
  for (size_t i = 0; i < num_children; ++i) {
    const ChildLayout& left_child = left.child_layouts[i];
    const ChildLayout& right_child = right.child_layouts[i];
    DCHECK_EQ(left_child.child_view, right_child.child_view);
    layout.child_layouts.emplace_back(
        ChildLayout{left_child.child_view,
                    InterpolateRect(left_child.bounds, right_child.bounds),
                    left_child.visible && right_child.visible});
  }
  return layout;
}

}  // namespace

InterpolatingLayoutManager::InterpolatingLayoutManager() {}
InterpolatingLayoutManager::~InterpolatingLayoutManager() = default;

InterpolatingLayoutManager& InterpolatingLayoutManager::SetOrientation(
    LayoutOrientation orientation) {
  if (orientation_ != orientation) {
    orientation_ = orientation;
    InvalidateLayout();
  }
  return *this;
}

void InterpolatingLayoutManager::AddLayoutInternal(
    std::unique_ptr<LayoutManagerBase> engine,
    const Span& interpolation_range) {
  DCHECK(engine);

  SyncStateTo(engine.get());
  auto result = embedded_layouts_.emplace(
      std::make_pair(interpolation_range, std::move(engine)));
  DCHECK(result.second) << "Cannot replace existing layout manager for "
                        << interpolation_range.ToString();

#if DCHECK_IS_ON()
  // Sanity checking to ensure interpolation ranges do not overlap (we can only
  // interpolate between two layouts currently).
  auto next = result.first;
  ++next;
  if (next != embedded_layouts_.end())
    DCHECK_GE(next->first.start(), interpolation_range.end());
  if (result.first != embedded_layouts_.begin()) {
    auto prev = result.first;
    --prev;
    DCHECK_LE(prev->first.end(), interpolation_range.start());
  }
#endif  // DCHECK_IS_ON()
}

LayoutManagerBase::ProposedLayout InterpolatingLayoutManager::GetProposedLayout(
    const SizeBounds& size_bounds) const {
  // Make sure a default engine is set.
  DCHECK(embedded_layouts_.find({0, 0}) != embedded_layouts_.end());

  ProposedLayout layout;

  const base::Optional<int> dimension =
      orientation_ == LayoutOrientation::kHorizontal ? size_bounds.width()
                                                     : size_bounds.height();

  // Find the larger layout that overlaps the target size.
  auto right_match = dimension ? embedded_layouts_.upper_bound({*dimension, 0})
                               : embedded_layouts_.end();
  DCHECK(right_match != embedded_layouts_.begin());
  --right_match;
  ProposedLayout right = right_match->second->GetProposedLayout(size_bounds);

  // If the target size falls in an interpolation range, get the other layout.
  if (dimension && right_match->first.end() > *dimension) {
    DCHECK(right_match != embedded_layouts_.begin());
    auto left_match = --(right_match);
    ProposedLayout left = left_match->second->GetProposedLayout(size_bounds);
    layout = Interpolate(left, right);
  } else {
    layout = std::move(right);
  }

  return layout;
}

void InterpolatingLayoutManager::InvalidateLayout() {
  LayoutManagerBase::InvalidateLayout();
  for (auto& embedded : embedded_layouts_)
    embedded.second->InvalidateLayout();
}

void InterpolatingLayoutManager::SetChildViewIgnoredByLayout(View* child_view,
                                                             bool ignored) {
  LayoutManagerBase::SetChildViewIgnoredByLayout(child_view, ignored);
  for (auto& embedded : embedded_layouts_)
    embedded.second->SetChildViewIgnoredByLayout(child_view, ignored);
}

void InterpolatingLayoutManager::Installed(View* host_view) {
  LayoutManagerBase::Installed(host_view);
  for (auto& embedded : embedded_layouts_)
    embedded.second->Installed(host_view);
}

void InterpolatingLayoutManager::ViewAdded(View* host_view, View* child_view) {
  LayoutManagerBase::ViewAdded(host_view, child_view);
  for (auto& embedded : embedded_layouts_)
    embedded.second->ViewAdded(host_view, child_view);
}

void InterpolatingLayoutManager::ViewRemoved(View* host_view,
                                             View* child_view) {
  LayoutManagerBase::ViewRemoved(host_view, child_view);
  for (auto& embedded : embedded_layouts_)
    embedded.second->ViewRemoved(host_view, child_view);
}

void InterpolatingLayoutManager::ViewVisibilitySet(View* host,
                                                   View* view,
                                                   bool visible) {
  LayoutManagerBase::ViewVisibilitySet(host, view, visible);
  for (auto& embedded : embedded_layouts_)
    embedded.second->ViewVisibilitySet(host, view, visible);
}

}  // namespace views
