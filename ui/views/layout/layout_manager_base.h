// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_LAYOUT_MANAGER_BASE_H_
#define UI_VIEWS_LAYOUT_LAYOUT_MANAGER_BASE_H_

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// Base class for layout managers that can do layout calculation separately
// from layout application. Derived classes must implement
// CalculateProposedLayout(). Used in interpolating and animating layouts.
class VIEWS_EXPORT LayoutManagerBase : public LayoutManager {
 public:
  // Represents layout information for a child view within a host being laid
  // out.
  struct VIEWS_EXPORT ChildLayout {
    View* child_view = nullptr;
    gfx::Rect bounds;
    bool visible = false;
  };

  // Contains a full layout specification for the children of the host view.
  struct VIEWS_EXPORT ProposedLayout {
    ProposedLayout();
    ~ProposedLayout();
    ProposedLayout(const ProposedLayout& other);
    ProposedLayout(ProposedLayout&& other);
    ProposedLayout& operator=(const ProposedLayout& other);
    ProposedLayout& operator=(ProposedLayout&& other);

    // The size of the host view given the size bounds for this layout. If both
    // dimensions of the size bounds are specified, this will be the same size.
    gfx::Size host_size;

    // Contains an entry for each child view included in the layout.
    std::vector<ChildLayout> child_layouts;
  };

  ~LayoutManagerBase() override;

  View* host_view() { return host_view_; }
  const View* host_view() const { return host_view_; }

  // Fetches a proposed layout for a host view with size |host_size|. If the
  // result had already been calculated, a cached value may be returned.
  ProposedLayout GetProposedLayout(const gfx::Size& host_size) const;

  // Excludes a specific view from the layout when doing layout calculations.
  // Useful when a child view is meant to be displayed but has its size and
  // position managed elsewhere in code. By default, all child views are
  // included in the layout unless they are hidden.
  virtual void SetChildViewIgnoredByLayout(View* child_view, bool ignored);
  virtual bool IsChildViewIgnoredByLayout(const View* child_view) const;

  // LayoutManager:
  gfx::Size GetPreferredSize(const View* host) const override;
  gfx::Size GetMinimumSize(const View* host) const override;
  int GetPreferredHeightForWidth(const View* host, int width) const override;
  void Layout(View* host) override;
  void InvalidateLayout() override;
  void Installed(View* host) override;
  void ViewAdded(View* host, View* view) override;
  void ViewRemoved(View* host, View* view) override;
  void ViewVisibilitySet(View* host, View* view, bool visible) override;

 protected:
  LayoutManagerBase();

  bool IsChildIncludedInLayout(const View* child) const;

  // Creates a proposed layout for the host view, including bounds and
  // visibility for all children currently included in the layout.
  virtual ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const = 0;

  // Applies |layout| to the children of the host view.
  void ApplyLayout(const ProposedLayout& layout);

  // Can be used by derived classes to ensure that state is correctly
  // transferred to child LayoutManagerBase instances in a composite layout
  // (interpolating or animating layouts, etc.)
  void SyncStateTo(LayoutManagerBase* other) const;

 private:
  // Holds bookkeeping data used to determine inclusion of children in the
  // layout.
  struct ChildInfo {
    bool can_be_visible = true;
    bool ignored = false;
  };

  View* host_view_ = nullptr;
  std::map<const View*, ChildInfo> child_infos_;

  // Do some really simple caching because layout generation can cost as much
  // as 1ms or more for complex views.
  mutable base::Optional<gfx::Size> minimum_size_;
  mutable base::Optional<gfx::Size> preferred_size_;
  mutable base::Optional<gfx::Size> last_height_for_width_;
  mutable base::Optional<gfx::Size> last_requested_size_;
  mutable ProposedLayout last_layout_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManagerBase);
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_LAYOUT_MANAGER_BASE_H_
