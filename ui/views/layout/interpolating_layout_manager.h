// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_INTERPOLATING_LAYOUT_MANAGER_H_
#define UI_VIEWS_LAYOUT_INTERPOLATING_LAYOUT_MANAGER_H_

#include <map>
#include <memory>
#include <utility>

#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_manager_base.h"

namespace views {

// Layout which interpolates between multiple embedded LayoutManagerBase
// layouts.
//
// An InterpolatingLayoutManager has a default layout, which applies at the
// smallest layout size along the layout's major axis (defined by |orientation|)
// and additional layouts, which phase in at some larger size. If only the
// default layout is set, the layout is functionally equivalent to the default
// layout.
//
// An example:
//
//   InterpolatingLayoutManager* e =
//       new InterpolatingLayoutManager(LayoutOrientation::kHorizontal);
//   e->SetDefaultLayout(std::make_unique<CompactLayout>());
//   e->AddLayout(std::make_unique<NormalLayout>(), 50, 50);
//   e->AddLayout(std::make_unique<SpaciousLayout>(), 100, 150);
//
// Now as the view expands, the different layouts are used:
//
// 0              50            100            150
// |   Compact    |    Normal    | Norm <~> Spa |  Spacious ->
//
// In the range from 100 to 150 (exclusive), an interpolation of the Normal and
// Spacious layouts is used. When interpolation happens in this way, the
// visibility of views is the conjunction of the visibilities in each layout, so
// if either layout hides a view then the interpolated layout also hides it.
// Since this can produce some unwanted visual results, we recommend making sure
// that over the interpolation range, visibility matches up between the layouts
// on either side.
//
// Note that behavior when interpolation ranges overlap is undefined, but will
// be guaranteed to at least be the result of mixing two adjacent layouts that
// fall over the range in a way that is not completely irrational.
class VIEWS_EXPORT InterpolatingLayoutManager : public LayoutManagerBase {
 public:
  InterpolatingLayoutManager();
  ~InterpolatingLayoutManager() override;

  InterpolatingLayoutManager& SetOrientation(LayoutOrientation orientation);
  LayoutOrientation orientation() const { return orientation_; }

  // Sets the default layout, which takes effect at zero size on the layout's
  // main axis, and continues to be active until the next layout phases in.
  //
  // This object retains ownership of the layout engine, but the method returns
  // a typed raw pointer to the added layout engine.
  template <class T>
  T* SetDefaultLayout(std::unique_ptr<T> layout_manager) {
    T* const temp = layout_manager.get();
    AddLayoutInternal(std::move(layout_manager), Span());
    return temp;
  }

  // Adds an additional layout which starts and finished phasing in at
  // |start_interpolation| and |end_interpolation|, respectively. Currently,
  // having more than one layout's interpolation range overlapping results in
  // undefined behavior.
  //
  // This object retains ownership of the layout engine, but the method returns
  // a typed raw pointer to the added layout engine.
  template <class T>
  T* AddLayout(std::unique_ptr<T> layout_manager,
               const Span& interpolation_range) {
    T* const temp = layout_manager.get();
    AddLayoutInternal(std::move(layout_manager), interpolation_range);
    return temp;
  }

  // CachingLayout:
  void InvalidateLayout() override;
  void SetChildViewIgnoredByLayout(View* child_view, bool ignored) override;
  void Installed(View* host_view) override;
  void ViewAdded(View* host_view, View* child_view) override;
  void ViewRemoved(View* host_view, View* child_view) override;
  void ViewVisibilitySet(View* host, View* view, bool visible) override;
  ProposedLayout GetProposedLayout(
      const SizeBounds& size_bounds) const override;

 private:
  void AddLayoutInternal(std::unique_ptr<LayoutManagerBase> layout,
                         const Span& interpolation_range);

  LayoutOrientation orientation_ = LayoutOrientation::kHorizontal;

  // Maps from interpolation range to embedded layout.
  std::map<Span, std::unique_ptr<LayoutManagerBase>> embedded_layouts_;

  DISALLOW_COPY_AND_ASSIGN(InterpolatingLayoutManager);
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_INTERPOLATING_LAYOUT_MANAGER_H_
