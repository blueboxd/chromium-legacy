// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/layout/interpolating_layout_manager.h"

#include <memory>
#include <utility>

#include "ui/gfx/animation/tween.h"
#include "ui/views/view.h"

InterpolatingLayoutManager::InterpolatingLayoutManager() {}
InterpolatingLayoutManager::~InterpolatingLayoutManager() = default;

InterpolatingLayoutManager& InterpolatingLayoutManager::SetOrientation(
    views::LayoutOrientation orientation) {
  if (orientation_ != orientation) {
    orientation_ = orientation;
    InvalidateLayout();
  }
  return *this;
}

void InterpolatingLayoutManager::AddLayoutInternal(
    std::unique_ptr<LayoutManagerBase> engine,
    const views::Span& interpolation_range) {
  DCHECK(engine);

  SyncStateTo(engine.get());
  auto result = embedded_layouts_.emplace(
      std::make_pair(interpolation_range, std::move(engine)));
  DCHECK(result.second) << "Cannot replace existing layout manager for "
                        << interpolation_range.ToString();

#if DCHECK_IS_ON()
  // Sanity checking to ensure interpolation ranges do not overlap (we can
  // only interpolate between two layouts currently).
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

InterpolatingLayoutManager::LayoutInterpolation
InterpolatingLayoutManager::GetInterpolation(
    const views::SizeBounds& size_bounds) const {
  DCHECK(!embedded_layouts_.empty());

  LayoutInterpolation result;

  const base::Optional<int> dimension =
      orientation_ == views::LayoutOrientation::kHorizontal
          ? size_bounds.width()
          : size_bounds.height();

  // Find the larger layout that overlaps the target size.
  auto match = dimension ? embedded_layouts_.upper_bound({*dimension, 0})
                         : embedded_layouts_.end();
  DCHECK(match != embedded_layouts_.begin())
      << "No layout set for primary dimension size "
      << (dimension ? *dimension : -1) << "; first layout starts at "
      << match->first.ToString();
  result.first = (--match)->second.get();

  // If the target size falls in an interpolation range, get the other layout.
  const views::Span& first_span = match->first;
  if (dimension && first_span.end() > *dimension) {
    DCHECK(match != embedded_layouts_.begin())
        << "Primary dimension size " << (dimension ? *dimension : -1)
        << " falls into interpolation range " << match->first.ToString()
        << " but there is no smaller layout to interpolate with.";
    result.second = (--match)->second.get();
    result.percent_second =
        float{first_span.end() - *dimension} / float{first_span.length()};
  }

  return result;
}

views::LayoutManagerBase::ProposedLayout
InterpolatingLayoutManager::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  // For interpolating layout we will never call this method except for fully-
  // specified sizes.
  DCHECK(size_bounds.width());
  DCHECK(size_bounds.height());
  const gfx::Size size(*size_bounds.width(), *size_bounds.height());

  const LayoutInterpolation interpolation = GetInterpolation(size_bounds);
  const ProposedLayout first = interpolation.first->GetProposedLayout(size);

  if (!interpolation.second)
    return first;

  // If the target size falls in an interpolation range, get the other layout.
  const ProposedLayout second = interpolation.second->GetProposedLayout(size);
  return Interpolate(interpolation.percent_second, first, second);
}

void InterpolatingLayoutManager::SetDefaultLayout(
    LayoutManagerBase* default_layout) {
  if (default_layout_ == default_layout)
    return;

  // Make sure we already own the layout.
  DCHECK(embedded_layouts_.end() !=
         std::find_if(embedded_layouts_.begin(), embedded_layouts_.end(),
                      [=](const auto& pair) {
                        return pair.second.get() == default_layout;
                      }));
  default_layout_ = default_layout;
  InvalidateLayout();
}

gfx::Size InterpolatingLayoutManager::GetPreferredSize(
    const views::View* host) const {
  DCHECK_EQ(host_view(), host);
  DCHECK(host);
  return GetDefaultLayout()->GetPreferredSize(host);
}

gfx::Size InterpolatingLayoutManager::GetMinimumSize(
    const views::View* host) const {
  DCHECK_EQ(host_view(), host);
  DCHECK(host);
  return GetSmallestLayout()->GetMinimumSize(host);
}

int InterpolatingLayoutManager::GetPreferredHeightForWidth(
    const views::View* host,
    int width) const {
  // It is in general not possible to determine what the correct height-for-
  // width trade-off is while interpolating between two already-generated
  // layouts because the values tend to rely on the behavior of individual child
  // views at specific dimensions.
  //
  // The two reasonable choices are to use the larger of the two values (with
  // the understanding that the height of the view may "pop" at the edge of the
  // interpolation range), or to interpolate between the heights and hope that
  // the result is fairly close to what we would want.
  //
  // We have opted for the second approach because it provides a smoother visual
  // experience; if this doesn't work in practice we can look at other options.

  const LayoutInterpolation interpolation =
      GetInterpolation({width, base::nullopt});
  const int first =
      interpolation.first->GetPreferredHeightForWidth(host, width);
  if (!interpolation.second)
    return first;

  const int second =
      interpolation.second->GetPreferredHeightForWidth(host, width);
  return gfx::Tween::LinearIntValueBetween(interpolation.percent_second, first,
                                           second);
}

void InterpolatingLayoutManager::InvalidateLayout() {
  LayoutManagerBase::InvalidateLayout();
  for (auto& embedded : embedded_layouts_)
    embedded.second->InvalidateLayout();
}

void InterpolatingLayoutManager::SetChildViewIgnoredByLayout(
    views::View* child_view,
    bool ignored) {
  LayoutManagerBase::SetChildViewIgnoredByLayout(child_view, ignored);
  for (auto& embedded : embedded_layouts_)
    embedded.second->SetChildViewIgnoredByLayout(child_view, ignored);
}

// static
views::LayoutManagerBase::ProposedLayout
InterpolatingLayoutManager::Interpolate(double value,
                                        const ProposedLayout& start,
                                        const ProposedLayout& target) {
  if (value >= 1.0)
    return target;

  ProposedLayout layout;

  // Interpolate the host size.
  layout.host_size =
      gfx::Tween::SizeValueBetween(value, start.host_size, target.host_size);

  // The views may not be listed in the same order and some views might be
  // omitted from either the |start| or |target| layout.
  std::map<const views::View*, size_t> start_view_to_index;
  for (size_t i = 0; i < start.child_layouts.size(); ++i)
    start_view_to_index.emplace(start.child_layouts[i].child_view, i);
  for (const ChildLayout& target_child : target.child_layouts) {
    // Try to match the view from the target with the view from the start.
    const auto start_match = start_view_to_index.find(target_child.child_view);
    if (start_match == start_view_to_index.end()) {
      // If there is no match, make the view present but invisible.
      layout.child_layouts.push_back({target_child.child_view, false});
    } else {
      // Tween the two layouts.
      const ChildLayout& start_child = start.child_layouts[start_match->second];
      layout.child_layouts.push_back(
          {target_child.child_view, start_child.visible && target_child.visible,
           gfx::Tween::RectValueBetween(value, start_child.bounds,
                                        target_child.bounds)});
    }
  }
  return layout;
}

void InterpolatingLayoutManager::Installed(views::View* host_view) {
  LayoutManagerBase::Installed(host_view);
  for (auto& embedded : embedded_layouts_)
    embedded.second->Installed(host_view);
}

void InterpolatingLayoutManager::ViewAdded(views::View* host_view,
                                           views::View* child_view) {
  LayoutManagerBase::ViewAdded(host_view, child_view);
  for (auto& embedded : embedded_layouts_)
    embedded.second->ViewAdded(host_view, child_view);
}

void InterpolatingLayoutManager::ViewRemoved(views::View* host_view,
                                             views::View* child_view) {
  LayoutManagerBase::ViewRemoved(host_view, child_view);
  for (auto& embedded : embedded_layouts_)
    embedded.second->ViewRemoved(host_view, child_view);
}

void InterpolatingLayoutManager::ViewVisibilitySet(views::View* host,
                                                   views::View* view,
                                                   bool visible) {
  LayoutManagerBase::ViewVisibilitySet(host, view, visible);
  for (auto& embedded : embedded_layouts_)
    embedded.second->ViewVisibilitySet(host, view, visible);
}

const views::LayoutManagerBase* InterpolatingLayoutManager::GetDefaultLayout()
    const {
  DCHECK(!embedded_layouts_.empty());
  return default_layout_ ? default_layout_
                         : embedded_layouts_.rbegin()->second.get();
}

const views::LayoutManagerBase* InterpolatingLayoutManager::GetSmallestLayout()
    const {
  DCHECK(!embedded_layouts_.empty());
  return embedded_layouts_.begin()->second.get();
}
