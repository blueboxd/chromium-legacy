// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_manager_base.h"

#include <utility>

#include "base/logging.h"
#include "ui/views/view.h"

namespace views {

LayoutManagerBase::ProposedLayout::ProposedLayout() = default;
LayoutManagerBase::ProposedLayout::ProposedLayout(const ProposedLayout& other) =
    default;
LayoutManagerBase::ProposedLayout::ProposedLayout(ProposedLayout&& other) =
    default;
LayoutManagerBase::ProposedLayout::~ProposedLayout() = default;
LayoutManagerBase::ProposedLayout& LayoutManagerBase::ProposedLayout::operator=(
    const ProposedLayout& other) = default;
LayoutManagerBase::ProposedLayout& LayoutManagerBase::ProposedLayout::operator=(
    ProposedLayout&& other) = default;

LayoutManagerBase::~LayoutManagerBase() = default;

gfx::Size LayoutManagerBase::GetPreferredSize(const View* host) const {
  DCHECK_EQ(host_view_, host);
  if (!preferred_size_)
    preferred_size_ = CalculateProposedLayout(SizeBounds()).host_size;
  return *preferred_size_;
}

gfx::Size LayoutManagerBase::GetMinimumSize(const View* host) const {
  DCHECK_EQ(host_view_, host);
  if (!minimum_size_)
    minimum_size_ = CalculateProposedLayout(SizeBounds(0, 0)).host_size;
  return *minimum_size_;
}

int LayoutManagerBase::GetPreferredHeightForWidth(const View* host,
                                                  int width) const {
  if (!last_height_for_width_ || last_height_for_width_->width() != width) {
    const int height = CalculateProposedLayout(SizeBounds(width, base::nullopt))
                           .host_size.height();
    last_height_for_width_ = gfx::Size(width, height);
  }

  return last_height_for_width_->height();
}

void LayoutManagerBase::Layout(View* host) {
  DCHECK_EQ(host_view_, host);
  const gfx::Size size = host->size();
  ApplyLayout(GetProposedLayout(size));
}

void LayoutManagerBase::InvalidateLayout() {
  minimum_size_.reset();
  preferred_size_.reset();
  last_height_for_width_.reset();
  last_requested_size_.reset();
}

LayoutManagerBase::ProposedLayout LayoutManagerBase::GetProposedLayout(
    const gfx::Size& host_size) const {
  if (!last_requested_size_ || *last_requested_size_ != host_size) {
    last_requested_size_ = host_size;
    last_layout_ = CalculateProposedLayout(SizeBounds(host_size));
  }
  return last_layout_;
}

void LayoutManagerBase::SetChildViewIgnoredByLayout(View* child_view,
                                                    bool ignored) {
  auto it = child_infos_.find(child_view);
  DCHECK(it != child_infos_.end());
  if (it->second.ignored == ignored)
    return;

  it->second.ignored = ignored;
  InvalidateLayout();
}

bool LayoutManagerBase::IsChildViewIgnoredByLayout(
    const View* child_view) const {
  auto it = child_infos_.find(child_view);
  DCHECK(it != child_infos_.end());
  return it->second.ignored;
}

void LayoutManagerBase::Installed(View* host_view) {
  DCHECK(host_view);
  DCHECK(!host_view_);
  DCHECK(child_infos_.empty());

  host_view_ = host_view;
  for (auto it = host_view->children().begin();
       it != host_view->children().end(); ++it) {
    child_infos_.emplace(*it, ChildInfo{(*it)->GetVisible(), false});
  }
}

void LayoutManagerBase::ViewAdded(View* host, View* view) {
  DCHECK_EQ(host_view_, host);
  DCHECK(!base::Contains(child_infos_, view));
  ChildInfo to_add{view->GetVisible(), false};
  child_infos_.emplace(view, to_add);
  if (to_add.can_be_visible)
    InvalidateLayout();
}

void LayoutManagerBase::ViewRemoved(View* host, View* view) {
  DCHECK_EQ(host_view_, host);
  auto it = child_infos_.find(view);
  DCHECK(it != child_infos_.end());
  const bool removed_visible = it->second.can_be_visible && !it->second.ignored;
  child_infos_.erase(it);
  if (removed_visible)
    InvalidateLayout();
}

void LayoutManagerBase::ViewVisibilitySet(View* host,
                                          View* view,
                                          bool visible) {
  DCHECK_EQ(host_view_, host);
  auto it = child_infos_.find(view);
  DCHECK(it != child_infos_.end());
  if (it->second.can_be_visible == visible)
    return;

  it->second.can_be_visible = visible;
  if (!it->second.ignored)
    InvalidateLayout();
}

LayoutManagerBase::LayoutManagerBase() = default;

bool LayoutManagerBase::IsChildIncludedInLayout(const View* child) const {
  const auto it = child_infos_.find(child);
  DCHECK(it != child_infos_.end());
  return !it->second.ignored && it->second.can_be_visible;
}

void LayoutManagerBase::ApplyLayout(const ProposedLayout& layout) {
  for (auto& child_layout : layout.child_layouts) {
    DCHECK_EQ(host_view_, child_layout.child_view->parent());

    // Since we have a non-const reference to the parent here, we can safely use
    // a non-const reference to the child.
    View* const child_view = child_layout.child_view;
    if (child_view->GetVisible() != child_layout.visible)
      SetViewVisibility(child_view, child_layout.visible);
    if (child_layout.visible)
      child_view->SetBoundsRect(child_layout.bounds);
  }
}

void LayoutManagerBase::SyncStateTo(LayoutManagerBase* other) const {
  if (host_view_) {
    other->Installed(host_view_);
    for (View* child_view : host_view_->children()) {
      const ChildInfo& child_info = child_infos_.find(child_view)->second;
      other->SetChildViewIgnoredByLayout(child_view, child_info.ignored);
      other->ViewVisibilitySet(host_view_, child_view,
                               child_info.can_be_visible);
    }
  }
}

}  // namespace views
