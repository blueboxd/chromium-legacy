// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/flex_layout.h"

#include <algorithm>
#include <functional>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/events/event_target.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_types_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

// Module-private declarations -------------------------------------------------

namespace views {

namespace internal {

namespace {

// Layout information for a specific child view in a proposed layout.
struct ChildLayout {
  ChildLayout(View* view, const FlexSpecification& flex)
      : view(view), flex(flex) {}
  ChildLayout(ChildLayout&& other) = default;

  View* view = nullptr;
  bool excluded = false;
  // Indicates whether external code has called SetVisible(false) on the view.
  bool hidden_by_owner = false;
  // Indicates whether the layout has chosen to display this child view.
  bool visible = false;
  // Start with zero size rather than unspecified size bounds because we start
  // all layouts with controls at their minimum allowed sizes.
  NormalizedSizeBounds available_size{0, 0};
  NormalizedSize preferred_size;
  NormalizedSize current_size;
  NormalizedRect actual_bounds;
  NormalizedInsets margins;
  NormalizedInsets internal_padding;
  FlexSpecification flex;

 private:
  // Copying this struct would be expensive and they only ever live in a vector
  // in Layout (see below) so we'll only allow move semantics.
  DISALLOW_COPY_AND_ASSIGN(ChildLayout);
};

// Represents a specific stored layout given a set of size bounds.
struct Layout {
  explicit Layout(int64_t layout_counter) : layout_id(layout_counter) {}

  // Indicates which layout pass this layout was created/last validated on. If
  // this number disagrees with FlexLayoutInternal::layout_counter_, we need to
  // re-validate the layout before using it.
  mutable uint32_t layout_id;
  std::vector<ChildLayout> child_layouts;
  NormalizedInsets interior_margin;
  NormalizedSizeBounds available_size;
  NormalizedSize total_size;

 private:
  DISALLOW_COPY_AND_ASSIGN(Layout);
};

// Preserves layouts for common layout requests, because we may be asked to
// recompute them many times but don't want to repeat the calculation.
//
// Specifically, we will be asked for:
//  - The layout's minimum size (bounds = {0, 0})
//  - The layout's preferred size (neither bound specified)
//  - The final layout at the dimensions of the host view (both bounds nonzero)
//
// There is also the possibility of a view with a flex layout being embedded in
// a view with a flex layout, where the inner view has a nontrivial flex
// specification. In that case, in order to handle e.g. labels in the inner
// view, the outer layout manager will ask for the inner view's preferred height
// for its width, which potentially involves another layout calculation.
// However, these calculations are transient - they only happen when the layout
// is recalculated or validated, and in the latter case only one set of bounds
// is evaluated. So we will keep exactly one of these layouts around.
//
// As a first pass, we'll store all of these specifically. In the future we can
// switch to using something more sophisticated, like an MRU cache, if we find
// we're thrashing for some other reason.
class LayoutCache {
 public:
  // Removes all saved layouts.
  void Clear() {
    minimum_.reset();
    preferred_.reset();
    intermediate_.layout.reset();
    current_.layout.reset();
  }

  // Gets a cached layout, or nullptr if no layout is cached for the specified
  // bounds.
  Layout* Get(const NormalizedSizeBounds& bounds) const {
    if (bounds == NormalizedSizeBounds(0, 0))
      return minimum_.get();
    if (bounds == NormalizedSizeBounds())
      return preferred_.get();
    if (bounds == intermediate_.bounds)
      return intermediate_.layout.get();
    if (bounds == current_.bounds)
      return current_.layout.get();
    return nullptr;
  }

  // Caches a layout associated with the specified bounds.
  void Put(const NormalizedSizeBounds& bounds, std::unique_ptr<Layout> layout) {
    if (bounds == NormalizedSizeBounds(0, 0)) {
      minimum_ = std::move(layout);
    } else if (bounds == NormalizedSizeBounds()) {
      preferred_ = std::move(layout);
    } else if (!bounds.cross() || !bounds.main()) {
      intermediate_.bounds = bounds;
      intermediate_.layout = std::move(layout);
    } else {
      current_.bounds = bounds;
      current_.layout = std::move(layout);
    }
  }

 private:
  struct CacheEntry {
    NormalizedSizeBounds bounds;
    std::unique_ptr<Layout> layout;
  };

  std::unique_ptr<Layout> minimum_;
  std::unique_ptr<Layout> preferred_;

  // TODO(dfried): consider replacing these with an MRUCache if this ends up
  // thrashing a lot (see note above).
  CacheEntry intermediate_;
  CacheEntry current_;
};

}  // anonymous namespace

// Private implementation ------------------------------------------------------

// Calculates and maintains 1D spacing between a sequence of child views.
class ChildViewSpacing {
 public:
  // Given the indices of two child views, returns the amount of space that
  // should be placed between them if they were adjacent. If the first index is
  // absent, uses the left edge of the parent container. If the second index is
  // absent, uses the right edge of the parent container.
  using GetViewSpacingCallback =
      base::RepeatingCallback<int(base::Optional<size_t>,
                                  base::Optional<size_t>)>;

  explicit ChildViewSpacing(GetViewSpacingCallback get_view_spacing);
  ChildViewSpacing(ChildViewSpacing&& other);
  ChildViewSpacing& operator=(ChildViewSpacing&& other);

  bool HasViewIndex(size_t view_index) const;
  int GetLeadingInset() const;
  int GetTrailingInset() const;
  int GetLeadingSpace(size_t view_index) const;
  int GetTotalSpace() const;

  // Returns the change in space required if the specified view index were
  // added.
  int GetAddDelta(size_t view_index) const;

  // Add the view at the specified index.
  //
  // If |new_leading| or |new_trailing| is specified, it will be set to the new
  // leading/trailing space for the view at the index that was added.
  void AddViewIndex(size_t view_index,
                    int* new_leading = nullptr,
                    int* new_trailing = nullptr);

 private:
  base::Optional<size_t> GetPreviousViewIndex(size_t view_index) const;
  base::Optional<size_t> GetNextViewIndex(size_t view_index) const;

  GetViewSpacingCallback get_view_spacing_;
  // Maps from view index to the leading spacing for that index.
  std::map<size_t, int> leading_spacings_;
  // The trailing space (space preceding the trailing margin).
  int trailing_space_;
};

ChildViewSpacing::ChildViewSpacing(GetViewSpacingCallback get_view_spacing)
    : get_view_spacing_(std::move(get_view_spacing)),
      trailing_space_(get_view_spacing_.Run(base::nullopt, base::nullopt)) {}

ChildViewSpacing::ChildViewSpacing(ChildViewSpacing&& other)
    : get_view_spacing_(std::move(other.get_view_spacing_)),
      leading_spacings_(std::move(other.leading_spacings_)),
      trailing_space_(other.trailing_space_) {}

ChildViewSpacing& ChildViewSpacing::operator=(ChildViewSpacing&& other) {
  if (this != &other) {
    get_view_spacing_ = std::move(other.get_view_spacing_);
    leading_spacings_ = std::move(other.leading_spacings_);
    trailing_space_ = other.trailing_space_;
  }
  return *this;
}

bool ChildViewSpacing::HasViewIndex(size_t view_index) const {
  return leading_spacings_.find(view_index) != leading_spacings_.end();
}

int ChildViewSpacing::GetLeadingInset() const {
  if (leading_spacings_.empty())
    return 0;
  return leading_spacings_.begin()->second;
}

int ChildViewSpacing::GetTrailingInset() const {
  return trailing_space_;
}

int ChildViewSpacing::GetLeadingSpace(size_t view_index) const {
  auto it = leading_spacings_.find(view_index);
  DCHECK(it != leading_spacings_.end());
  return it->second;
}

int ChildViewSpacing::GetTotalSpace() const {
  return std::accumulate(
      leading_spacings_.cbegin(), leading_spacings_.cend(), trailing_space_,
      [](int total, const auto& value) { return total + value.second; });
}

int ChildViewSpacing::GetAddDelta(size_t view_index) const {
  DCHECK(!HasViewIndex(view_index));
  base::Optional<size_t> prev = GetPreviousViewIndex(view_index);
  base::Optional<size_t> next = GetNextViewIndex(view_index);
  const int old_spacing = next ? GetLeadingSpace(*next) : GetTrailingInset();
  const int new_spacing = get_view_spacing_.Run(prev, view_index) +
                          get_view_spacing_.Run(view_index, next);
  return new_spacing - old_spacing;
}

void ChildViewSpacing::AddViewIndex(size_t view_index,
                                    int* new_leading,
                                    int* new_trailing) {
  DCHECK(!HasViewIndex(view_index));
  base::Optional<size_t> prev = GetPreviousViewIndex(view_index);
  base::Optional<size_t> next = GetNextViewIndex(view_index);

  const int leading_space = get_view_spacing_.Run(prev, view_index);
  const int trailing_space = get_view_spacing_.Run(view_index, next);
  leading_spacings_[view_index] = leading_space;
  if (next)
    leading_spacings_[*next] = trailing_space;
  else
    trailing_space_ = trailing_space;

  if (new_leading)
    *new_leading = leading_space;
  if (new_trailing)
    *new_trailing = trailing_space;
}

base::Optional<size_t> ChildViewSpacing::GetPreviousViewIndex(
    size_t view_index) const {
  const auto it = leading_spacings_.lower_bound(view_index);
  if (it == leading_spacings_.begin())
    return base::nullopt;
  return std::prev(it)->first;
}

base::Optional<size_t> ChildViewSpacing::GetNextViewIndex(
    size_t view_index) const {
  const auto it = leading_spacings_.upper_bound(view_index);
  if (it == leading_spacings_.end())
    return base::nullopt;
  return it->first;
}

// Holds child-view-specific layout parameters that are not stored in the
// properties system.
//
// We should consider storing some or all of these in the properites system.
struct ChildLayoutParams {
  bool excluded = false;
  bool hidden_by_owner = false;
};

// Internal data structure and functionality for FlexLayout so we don't have to
// declare a bunch of classes and data in the .h file.
class FlexLayoutInternal {
 public:
  explicit FlexLayoutInternal(FlexLayout* layout)
      : layout_(*layout) {}  //, cached_layouts_(kMaxCachedLayouts) {}

  // Suggests that the current layout needs to be recalculated. Setting |force|
  // to true indicates that we know all of the cached layouts are invalid and we
  // should discard them; otherwise we will keep them and re-validate on the
  // next layout pass.
  void InvalidateLayout(bool force);

  // Gets the proposed layout for a set of size bounds. Returns a cached layout
  // if one is present and valid.
  const Layout& CalculateLayout(const SizeBounds& bounds);

  // Applies an existing layout to all child views, with the appropriate
  // current alignment.
  void DoLayout(const Layout& layout, const gfx::Rect& bounds);

 private:
  // Maps a flex order (lower = allocated first, and therefore higher priority)
  // to the indices of child views within that order that can flex.
  // See FlexSpecification::order().
  using FlexOrderToViewIndexMap = std::map<int, std::vector<size_t>>;

  LayoutOrientation orientation() const { return layout_.orientation(); }

  // Determines whether a layout is still valid.
  bool IsLayoutValid(const Layout& cached_layout) const;

  // Creates a brand new layout from the available |bounds|.
  // Call DoLayout() to actually apply the layout.
  const Layout& CalculateNewLayout(const NormalizedSizeBounds& bounds);

  // Applies flex rules to each view in a layout, updating |layout| and
  // |child_spacing|.
  //
  // If |expandable_views| is specified, any view requesting more than its
  // preferred size will be clamped to its preferred size and be added to
  // |expandable_views| for later processing after all other flex space has been
  // allocated.
  //
  // Typically, this method will be called once with |expandable_views| set and
  // then again with it null to allocate the remaining space.
  void AllocateFlexSpace(
      Layout* layout,
      ChildViewSpacing* child_spacing,
      const NormalizedSizeBounds& bounds,
      const FlexOrderToViewIndexMap& order_to_index,
      FlexOrderToViewIndexMap* expandable_views = nullptr) const;

  // Calculates the position of each child view and the size of the overall
  // layout based on tentative visibilities and sizes for each child.
  void UpdateLayoutFromChildren(Layout* layout,
                                ChildViewSpacing* child_spacing,
                                const NormalizedSizeBounds& bounds) const;

  // Calculates the cross-layout space available to a view based on the
  // available space and margins.
  base::Optional<int> GetAvailableCrossAxisSize(
      const Layout& layout,
      size_t child_index,
      const NormalizedSizeBounds& bounds) const;

  // Calculates a margin between two child views based on each's margin and any
  // internal padding present in one or both elements. Uses properties of the
  // layout, like whether adjacent margins should be collapsed.
  int CalculateMargin(int margin1, int margin2, int internal_padding) const;

  // Calculates the preferred spacing between two child views, or between a
  // view edge and the first or last visible child views.
  int CalculateChildSpacing(const Layout& layout,
                            base::Optional<size_t> child1_index,
                            base::Optional<size_t> child2_index) const;

  // Fetches the layout-specific value for a child view, then the layout-default
  // value, then the global default value if none of the others are present.
  template <typename T>
  T GetViewProperty(const View* view,
                    const ui::ClassProperty<T*>* property) const;

  FlexLayout& layout_;

  // Instead of marking each layout as dirty/needing validation, we instead keep
  // a value that changes every time InvalidateLayout() is called. If the value
  // stored in a cached layout doesn't match this value, we validate it and
  // update the value. We use an int64_t because the odds of spurious collision
  // (i.e. the counter wrapping back around to the *exact* same value before
  // validating) are effectively zero.
  uint32_t layout_counter_ = 0;
  LayoutCache layout_cache_;

  DISALLOW_COPY_AND_ASSIGN(FlexLayoutInternal);
};

void FlexLayoutInternal::InvalidateLayout(bool force) {
  ++layout_counter_;
  if (force)
    layout_cache_.Clear();
}

const Layout& FlexLayoutInternal::CalculateLayout(const SizeBounds& bounds) {
  // If bounds are smaller than the minimum cross axis size, expand them.
  NormalizedSizeBounds normalized_bounds = Normalize(orientation(), bounds);
  if (normalized_bounds.cross() &&
      *normalized_bounds.cross() < layout_.minimum_cross_axis_size()) {
    normalized_bounds = NormalizedSizeBounds{normalized_bounds.main(),
                                             layout_.minimum_cross_axis_size()};
  }

  // See if we have a cached layout already that is still valid.
  Layout* const cached_layout = layout_cache_.Get(normalized_bounds);
  if (cached_layout && IsLayoutValid(*cached_layout))
    return *cached_layout;

  // Calculate a new layout from scratch.
  return CalculateNewLayout(normalized_bounds);
}

void FlexLayoutInternal::DoLayout(const Layout& layout,
                                  const gfx::Rect& bounds) {
  NormalizedPoint start = Normalize(orientation(), bounds.origin());

  // Apply main axis alignment.
  const int excess_main =
      Normalize(orientation(), bounds.size()).main() - layout.total_size.main();
  switch (layout_.main_axis_alignment()) {
    case LayoutAlignment::kStart:
      break;
    case LayoutAlignment::kCenter:
      start.set_main(start.main() + excess_main / 2);
      break;
    case LayoutAlignment::kEnd:
      start.set_main(start.main() + excess_main);
      break;
    case LayoutAlignment::kStretch:
      NOTIMPLEMENTED() << "Main axis stretch/justify is not yet supported.";
      break;
  }

  // Position controls within the client area.
  for (const ChildLayout& child_layout : layout.child_layouts) {
    if (child_layout.excluded)
      continue;
    if (child_layout.visible != child_layout.view->GetVisible())
      layout_.SetViewVisibility(child_layout.view, child_layout.visible);
    if (child_layout.visible) {
      NormalizedRect actual = child_layout.actual_bounds;
      actual.Offset(start.main(), start.cross());
      gfx::Rect denormalized = Denormalize(orientation(), actual);
      child_layout.view->SetBoundsRect(denormalized);
    }
  }
}

base::Optional<int> FlexLayoutInternal::GetAvailableCrossAxisSize(
    const Layout& layout,
    size_t child_index,
    const NormalizedSizeBounds& bounds) const {
  if (!bounds.cross())
    return base::nullopt;

  const ChildLayout& child_layout = layout.child_layouts[child_index];
  const int leading_margin =
      CalculateMargin(layout.interior_margin.cross_leading(),
                      child_layout.margins.cross_leading(),
                      child_layout.internal_padding.cross_leading());
  const int trailing_margin =
      CalculateMargin(layout.interior_margin.cross_trailing(),
                      child_layout.margins.cross_trailing(),
                      child_layout.internal_padding.cross_trailing());
  return std::max(0, *bounds.cross() - (leading_margin + trailing_margin));
}

int FlexLayoutInternal::CalculateMargin(int margin1,
                                        int margin2,
                                        int internal_padding) const {
  const int result = layout_.collapse_margins() ? std::max(margin1, margin2)
                                                : margin1 + margin2;
  return std::max(0, result - internal_padding);
}

int FlexLayoutInternal::CalculateChildSpacing(
    const Layout& layout,
    base::Optional<size_t> child1_index,
    base::Optional<size_t> child2_index) const {
  const int left_margin =
      child1_index ? layout.child_layouts[*child1_index].margins.main_trailing()
                   : layout.interior_margin.main_leading();
  const int right_margin =
      child2_index ? layout.child_layouts[*child2_index].margins.main_leading()
                   : layout.interior_margin.main_trailing();
  const int left_padding =
      child1_index
          ? layout.child_layouts[*child1_index].internal_padding.main_trailing()
          : 0;
  const int right_padding =
      child2_index
          ? layout.child_layouts[*child2_index].internal_padding.main_leading()
          : 0;

  return CalculateMargin(left_margin, right_margin,
                         left_padding + right_padding);
}

void FlexLayoutInternal::UpdateLayoutFromChildren(
    Layout* layout,
    ChildViewSpacing* child_spacing,
    const NormalizedSizeBounds& bounds) const {
  // Calculate starting minimum for cross-axis size.
  int min_cross_size =
      std::max(layout_.minimum_cross_axis_size(),
               CalculateMargin(layout->interior_margin.cross_leading(),
                               layout->interior_margin.cross_trailing(), 0));
  layout->total_size = NormalizedSize(0, min_cross_size);

  // For cases with a non-zero cross-axis bound, the objective is to fit the
  // layout into that precise size, not to determine what size we need.
  bool force_cross_size = false;
  if (bounds.cross() && *bounds.cross() > 0) {
    layout->total_size.SetToMax(0, *bounds.cross());
    force_cross_size = true;
  }

  std::vector<Inset1D> cross_spacings(layout->child_layouts.size());
  for (size_t i = 0; i < layout->child_layouts.size(); ++i) {
    ChildLayout& child_layout = layout->child_layouts[i];

    // We don't have to deal with excluded or invisible children.
    if (child_layout.excluded || !child_layout.visible)
      continue;

    // Update the cross-axis margins and if necessary, the size.
    Inset1D& cross_spacing = cross_spacings[i];
    cross_spacing.set_leading(
        CalculateMargin(layout->interior_margin.cross_leading(),
                        child_layout.margins.cross_leading(),
                        child_layout.internal_padding.cross_leading()));
    cross_spacing.set_trailing(
        CalculateMargin(layout->interior_margin.cross_trailing(),
                        child_layout.margins.cross_trailing(),
                        child_layout.internal_padding.cross_trailing()));

    if (!force_cross_size) {
      const int cross_size = std::min(child_layout.current_size.cross(),
                                      child_layout.preferred_size.cross());
      layout->total_size.SetToMax(0, cross_spacing.size() + cross_size);
    }

    // Calculate main-axis size and upper-left main axis coordinate.
    int leading_space;
    if (child_spacing->HasViewIndex(i))
      leading_space = child_spacing->GetLeadingSpace(i);
    else
      child_spacing->AddViewIndex(i, &leading_space);
    layout->total_size.Enlarge(leading_space, 0);

    const int size_main = child_layout.current_size.main();
    child_layout.actual_bounds.set_origin_main(layout->total_size.main());
    child_layout.actual_bounds.set_size_main(size_main);
    layout->total_size.Enlarge(size_main, 0);
  }

  // Add the end margin.
  layout->total_size.Enlarge(child_spacing->GetTrailingInset(), 0);

  // Calculate cross-axis positioning based on the cross margins and size that
  // were calculated above.
  const Span cross_span(0, layout->total_size.cross());
  for (size_t i = 0; i < layout->child_layouts.size(); ++i) {
    ChildLayout& child_layout = layout->child_layouts[i];

    // We don't have to deal with excluded or invisible children.
    if (child_layout.excluded || !child_layout.visible)
      continue;

    // Start with a size appropriate for the child view. For child views which
    // can become larger than the preferred size, start with the preferred size
    // and let the alignment operation (specifically, if the alignment is set to
    // kStretch) grow the child view.
    const int starting_cross_size = std::min(
        child_layout.current_size.cross(), child_layout.preferred_size.cross());
    child_layout.actual_bounds.set_size_cross(starting_cross_size);
    child_layout.actual_bounds.AlignCross(
        cross_span, layout_.cross_axis_alignment(), cross_spacings[i]);
  }
}

void FlexLayoutInternal::AllocateFlexSpace(
    Layout* layout,
    ChildViewSpacing* child_spacing,
    const NormalizedSizeBounds& bounds,
    const FlexOrderToViewIndexMap& order_to_index,
    FlexOrderToViewIndexMap* expandable_views) const {
  // Step through each flex priority allocating as much remaining space as
  // possible to each flex view.
  for (const auto& flex_elem : order_to_index) {
    // Check to see we haven't filled available space.
    int remaining = *bounds.main() - layout->total_size.main();
    if (remaining <= 0)
      break;
    const int flex_order = flex_elem.first;

    // The flex algorithm we're using works as follows:
    //  * For each child view at a particular flex order:
    //    - Calculate the percentage of the remaining flex space to allocate
    //      based on the ratio of its weight to the total unallocated weight
    //      at that order.
    //    - If the child view is already visible (it will be at its minimum
    //      size, which may or may not be zero), add the space the child is
    //      already taking up.
    //    - If the child view is not visible and adding it would introduce
    //      additional margin space between child views, subtract that
    //      additional space from the amount available.
    //    - Ask the child view's flex rule how large it would like to be
    //      within the space available.
    //    - If the child view would like to be larger, make it so, and
    //      subtract the additional space consumed by the child and its
    //      margins from the total remaining flex space.
    //
    // Note that this algorithm isn't *perfect* for specific cases, which are
    // noted below; namely when margins very asymmetrical the sizing of child
    // views can be slightly different from what would otherwise be expected.
    // We have a TODO to look at ways of making this algorithm more "fair" in
    // the future (but in the meantime most issues can be resolved by setting
    // reasonable margins and by using flex order).

    // Flex children at this priority order.
    int flex_total = std::accumulate(
        flex_elem.second.begin(), flex_elem.second.end(), 0,
        [layout](int total, size_t index) {
          return total + layout->child_layouts[index].flex.weight();
        });

    // Note: because the child views are evaluated in order, if preferred
    // minimum sizes are not consistent across a single priority expanding
    // the parent control could result in children swapping visibility.
    // We currently consider this user error; if the behavior is not
    // desired, prioritize the child views' flex.
    bool dirty = false;
    for (auto index_it = flex_elem.second.begin();
         remaining >= 0 && index_it != flex_elem.second.end(); ++index_it) {
      const size_t view_index = *index_it;

      ChildLayout& child_layout = layout->child_layouts[view_index];

      // Offer a share of the remaining space to the view.
      int flex_amount;
      if (child_layout.flex.weight() > 0) {
        const int flex_weight = child_layout.flex.weight();
        // Round up so we give slightly greater weight to earlier views.
        flex_amount =
            int{std::ceil((float{remaining} * flex_weight) / flex_total)};
        flex_total -= flex_weight;
      } else {
        flex_amount = remaining;
      }

      // If the layout was previously invisible, then making it visible
      // may result in the addition of margin space, so we'll have to
      // recalculate the margins on either side of this view. The change in
      // margin space (if any) counts against the child view's flex space
      // allocation.
      //
      // Note: In cases where the layout's internal margins and/or the child
      // views' margins are wildly different sizes, subtracting the full delta
      // out of the available space can cause the first view to be smaller
      // than we would expect (see TODOs in unit tests for examples). We
      // should look into ways to make this "feel" better (but in the
      // meantime, please try to specify reasonable margins).
      const int margin_delta = child_spacing->HasViewIndex(view_index)
                                   ? 0
                                   : child_spacing->GetAddDelta(view_index);

      // This is the space on the main axis that was already allocated to the
      // child view; it will be added to the total flex space for the child
      // view since it is considered a fixed overhead of the layout if it is
      // nonzero.
      const int old_size =
          child_layout.visible ? child_layout.current_size.main() : 0;

      // Offer the modified flex space to the child view and see how large it
      // wants to be (or if it wants to be visible at that size at all).
      const NormalizedSizeBounds available(
          flex_amount + old_size - margin_delta,
          child_layout.available_size.cross());
      NormalizedSize new_size = Normalize(
          orientation(),
          child_layout.flex.rule().Run(child_layout.view,
                                       Denormalize(orientation(), available)));
      if (new_size.main() <= 0)
        continue;

      // Limit the expansion of views past their preferred size in the first
      // pass so that enough space is available for lower-priority views. Save
      // them to |expandable_views| so that the remaining space can be allocated
      // later.
      if (expandable_views &&
          new_size.main() >= child_layout.preferred_size.main()) {
        (*expandable_views)[flex_order].push_back(view_index);
        new_size.set_main(child_layout.preferred_size.main());
      }

      // If the amount of space claimed increases (but is still within
      // bounds set by our flex rule) we can make the control visible and
      // claim the additional space.
      const int to_deduct = (new_size.main() - old_size) + margin_delta;
      DCHECK_GE(to_deduct, 0);
      if (to_deduct > 0 && to_deduct <= remaining) {
        child_layout.available_size = available;
        child_layout.current_size = new_size;
        child_layout.visible = true;
        remaining -= to_deduct;
        if (!child_spacing->HasViewIndex(view_index))
          child_spacing->AddViewIndex(view_index);
        dirty = true;
      }
    }

    // Reposition the child controls (taking margins into account) and
    // calculate remaining space.
    if (dirty)
      UpdateLayoutFromChildren(layout, child_spacing, bounds);
  }
}

const Layout& FlexLayoutInternal::CalculateNewLayout(
    const NormalizedSizeBounds& bounds) {
  DCHECK(!bounds.cross() ||
         *bounds.cross() >= layout_.minimum_cross_axis_size());
  std::unique_ptr<Layout> layout = std::make_unique<Layout>(layout_counter_);
  layout->interior_margin = Normalize(orientation(), layout_.interior_margin());
  FlexOrderToViewIndexMap order_to_view_index;
  const bool main_axis_bounded = bounds.main().has_value();

  // Step through the children, creating placeholder layout view elements
  // and setting up initial minimal visibility.
  View* const view = layout_.host();
  for (size_t i = 0; i < view->children().size(); ++i) {
    View* child = view->children()[i];
    layout->child_layouts.emplace_back(
        child, GetViewProperty(child, views::kFlexBehaviorKey));
    ChildLayout& child_layout = layout->child_layouts.back();

    child_layout.excluded = layout_.IsViewExcluded(child);
    if (child_layout.excluded)
      continue;

    child_layout.margins =
        Normalize(orientation(), GetViewProperty(child, views::kMarginsKey));
    child_layout.internal_padding = Normalize(
        orientation(), GetViewProperty(child, views::kInternalPaddingKey));
    child_layout.preferred_size =
        Normalize(orientation(), child->GetPreferredSize());

    child_layout.hidden_by_owner = layout_.IsHiddenByOwner(child);
    child_layout.visible = !child_layout.hidden_by_owner;
    if (child_layout.hidden_by_owner)
      continue;

    // gfx::Size calculation depends on whether flex is allowed.
    if (main_axis_bounded) {
      child_layout.available_size = {
          0, GetAvailableCrossAxisSize(*layout, i, bounds)};
      child_layout.current_size = Normalize(
          orientation(),
          child_layout.flex.rule().Run(
              child, Denormalize(orientation(), child_layout.available_size)));

      // We should revisit whether this is a valid assumption for text views
      // in vertical layouts.
      DCHECK_GE(child_layout.preferred_size.main(),
                child_layout.current_size.main());

      // Keep track of non-hidden flex controls.
      const bool can_flex =
          (child_layout.flex.weight() > 0 &&
           child_layout.preferred_size.main() > 0) ||
          child_layout.current_size.main() < child_layout.preferred_size.main();
      if (can_flex)
        order_to_view_index[child_layout.flex.order()].push_back(i);

    } else {
      // All non-flex or unbounded controls get preferred size.
      child_layout.current_size = child_layout.preferred_size;
    }

    child_layout.visible = child_layout.current_size.main() > 0;

    // Actual size and positioning will be set during the final layout
    // calculation.
  }

  // Do the initial layout update, calculating spacing between children.
  ChildViewSpacing child_spacing(
      base::BindRepeating(&FlexLayoutInternal::CalculateChildSpacing,
                          base::Unretained(this), std::cref(*layout)));
  UpdateLayoutFromChildren(layout.get(), &child_spacing, bounds);

  if (main_axis_bounded && !order_to_view_index.empty()) {
    // Flex up to preferred size.
    FlexOrderToViewIndexMap expandable_views;
    AllocateFlexSpace(layout.get(), &child_spacing, bounds, order_to_view_index,
                      &expandable_views);

    // Flex views that can exceed their preferred size.
    if (!expandable_views.empty())
      AllocateFlexSpace(layout.get(), &child_spacing, bounds, expandable_views);
  }

  const Layout& result = *layout;
  layout_cache_.Put(bounds, std::move(layout));
  return result;
}

bool FlexLayoutInternal::IsLayoutValid(const Layout& cached_layout) const {
  // If we've already evaluated a layout for this size and not been
  // invalidated since then, then this is a valid layout.
  if (cached_layout.layout_id == layout_counter_)
    return true;

  // Need to compare preferred child sizes with what we're seeing.
  View* const view = layout_.host();
  auto iter = view->children().cbegin();
  for (const ChildLayout& proposed_view_layout : cached_layout.child_layouts) {
    // Check that there is another child and that it's the view we expect.
    DCHECK(iter != view->children().cend())
        << "Child views should not be removed without clearing the cache.";

    const View* child = *iter++;

    // Ensure child views have not been reordered.
    if (child != proposed_view_layout.view)
      return false;

    // Ignore hidden and excluded views, unless their status has changed.
    const bool excluded = layout_.IsViewExcluded(child);
    if (proposed_view_layout.excluded != excluded)
      return false;
    if (excluded)
      continue;

    const bool hidden_by_owner = layout_.IsHiddenByOwner(child);
    if (proposed_view_layout.hidden_by_owner != hidden_by_owner)
      return false;
    if (hidden_by_owner)
      continue;

    // Sanity check that a child's visibility hasn't been modified outside
    // the layout manager.
    if (proposed_view_layout.visible != child->GetVisible())
      return false;

    if (proposed_view_layout.visible) {
      // Check that view margins haven't changed for visible controls.
      if (GetViewProperty(child, views::kMarginsKey) !=
          Denormalize(orientation(), proposed_view_layout.margins))
        return false;

      // Same for internal padding.
      if (GetViewProperty(child, views::kInternalPaddingKey) !=
          Denormalize(orientation(), proposed_view_layout.internal_padding))
        return false;
    }

    // Check that the control still has the same preferred size given its
    // flex and visibility.
    if (child->GetPreferredSize() !=
        Denormalize(orientation(), proposed_view_layout.preferred_size))
      return false;

    const FlexSpecification child_flex =
        GetViewProperty(child, views::kFlexBehaviorKey);
    const gfx::Size preferred = child_flex.rule().Run(
        child, Denormalize(orientation(), proposed_view_layout.available_size));
    if (preferred !=
        Denormalize(orientation(), proposed_view_layout.current_size))
      return false;
  }

  DCHECK(iter == view->children().cend())
      << "Child views should not be added without clearing the cache.";

  // This layout is still valid. Update the layout counter to show it's valid
  // in the current layout context.
  cached_layout.layout_id = layout_counter_;
  return true;
}

template <typename T>
T FlexLayoutInternal::GetViewProperty(
    const View* view,
    const ui::ClassProperty<T*>* property) const {
  T* found_value = view->GetProperty(property);
  if (found_value)
    return *found_value;
  found_value = layout_.layout_defaults_.GetProperty(property);
  if (found_value)
    return *found_value;
  return T();
}

}  // namespace internal

// FlexLayout
// -------------------------------------------------------------------

FlexLayout::PropertyHandler::PropertyHandler(
    internal::FlexLayoutInternal* layout)
    : layout_(layout) {}

void FlexLayout::PropertyHandler::AfterPropertyChange(const void* key,
                                                      int64_t old_value) {
  layout_->InvalidateLayout(false);
}

FlexLayout::FlexLayout()
    : internal_(std::make_unique<internal::FlexLayoutInternal>(this)),
      layout_defaults_(internal_.get()) {}

FlexLayout::~FlexLayout() = default;

FlexLayout& FlexLayout::SetOrientation(LayoutOrientation orientation) {
  if (orientation != orientation_) {
    orientation_ = orientation;
    internal_->InvalidateLayout(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetCollapseMargins(bool collapse_margins) {
  if (collapse_margins != collapse_margins_) {
    collapse_margins_ = collapse_margins;
    internal_->InvalidateLayout(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetMainAxisAlignment(
    LayoutAlignment main_axis_alignment) {
  DCHECK_NE(main_axis_alignment, LayoutAlignment::kStretch)
      << "Main axis stretch/justify is not yet supported.";
  if (main_axis_alignment_ != main_axis_alignment) {
    main_axis_alignment_ = main_axis_alignment;
    internal_->InvalidateLayout(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetCrossAxisAlignment(
    LayoutAlignment cross_axis_alignment) {
  if (cross_axis_alignment_ != cross_axis_alignment) {
    cross_axis_alignment_ = cross_axis_alignment;
    internal_->InvalidateLayout(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetInteriorMargin(const gfx::Insets& interior_margin) {
  if (interior_margin_ != interior_margin) {
    interior_margin_ = interior_margin;
    internal_->InvalidateLayout(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetMinimumCrossAxisSize(int size) {
  if (minimum_cross_axis_size_ != size) {
    minimum_cross_axis_size_ = size;
    internal_->InvalidateLayout(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetViewExcluded(const View* view, bool excluded) {
  auto it = child_params_.find(view);
  DCHECK(it != child_params_.end());
  if (excluded != it->second.excluded) {
    it->second.excluded = excluded;
    InvalidateLayout();
  }
  return *this;
}

bool FlexLayout::IsViewExcluded(const View* view) const {
  auto it = child_params_.find(view);
  DCHECK(it != child_params_.end());
  return it->second.excluded;
}

bool FlexLayout::IsHiddenByOwner(const View* view) const {
  auto it = child_params_.find(view);
  DCHECK(it != child_params_.end());
  return it->second.hidden_by_owner;
}

gfx::Size FlexLayout::GetPreferredSize(const View* host) const {
  DCHECK_EQ(host_, host);
  return GetPreferredSize(SizeBounds());
}

int FlexLayout::GetPreferredHeightForWidth(const View* host, int width) const {
  DCHECK_EQ(host_, host);
  return GetPreferredSize(SizeBounds{width, base::nullopt}).height();
}

gfx::Size FlexLayout::GetMinimumSize(const View* host) const {
  DCHECK_EQ(host_, host);
  return GetPreferredSize(SizeBounds{0, 0});
}

void FlexLayout::InvalidateLayout() {
  internal_->InvalidateLayout(false);
}

void FlexLayout::Installed(View* host) {
  DCHECK(!host_);
  host_ = host;
  // Add all the existing children when the layout manager is installed.
  // If new children are added, ViewAdded() will be called and we'll add data
  // there.
  for (View* child : host->children()) {
    internal::ChildLayoutParams child_layout_params;
    child_layout_params.hidden_by_owner = !child->GetVisible();
    child_params_.emplace(child, child_layout_params);
  }
}

void FlexLayout::ViewAdded(View* host, View* view) {
  DCHECK_EQ(host_, host);
  internal::ChildLayoutParams child_layout_params;
  child_layout_params.hidden_by_owner = !view->GetVisible();
  child_params_.emplace(view, child_layout_params);
  internal_->InvalidateLayout(true);
}

void FlexLayout::ViewRemoved(View* host, View* view) {
  child_params_.erase(view);
  internal_->InvalidateLayout(true);
}

void FlexLayout::ViewVisibilitySet(View* host, View* view, bool visible) {
  DCHECK_EQ(host, host_);
  DCHECK_EQ(view->parent(), host);
  auto it = child_params_.find(view);
  DCHECK(it != child_params_.end());
  const bool hide = !visible;
  if (it->second.hidden_by_owner != hide) {
    it->second.hidden_by_owner = hide;
    if (!it->second.excluded) {
      // It's not always obvious to our host that an owner of a child view
      // changing its visibility could change a layout. So we'll notify the host
      // view that its layout is potentially invalid, and it will in turn call
      // InvalidateLayout() on the layout manager (i.e. this object).
      host_->InvalidateLayout();
    }
  }
}

// Retrieve the preferred size for the control in the given bounds.
gfx::Size FlexLayout::GetPreferredSize(const SizeBounds& bounds) const {
  if (!host_)
    return gfx::Size();

  const gfx::Insets insets = host_->GetInsets();
  SizeBounds content_bounds = bounds;
  content_bounds.Enlarge(-insets.width(), -insets.height());
  const internal::Layout& proposed_layout =
      internal_->CalculateLayout(content_bounds);
  gfx::Size result = Denormalize(orientation(), proposed_layout.total_size);
  result.Enlarge(insets.width(), insets.height());
  return result;
}

void FlexLayout::Layout(View* host) {
  DCHECK_EQ(host, host_);
  // TODO(dfried): for a handful of views which override GetInsets(), the way
  // this method is implemented in View may be incorrect. Please revisit.
  gfx::Rect bounds = host_->GetContentsBounds();
  const internal::Layout& layout =
      internal_->CalculateLayout(SizeBounds(bounds.size()));

  internal_->DoLayout(layout, bounds);
}

}  // namespace views
