// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/custom_tab.h"

#include <memory>
#include <string>
#include <utility>

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/exo/surface.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc {

namespace {

// Enumerates surfaces under the window.
void EnumerateSurfaces(aura::Window* window, std::vector<exo::Surface*>* out) {
  auto* surface = exo::Surface::AsSurface(window);
  if (surface)
    out->push_back(surface);
  for (aura::Window* child : window->children())
    EnumerateSurfaces(child, out);
}

}  // namespace

CustomTab::CustomTab(aura::Window* arc_app_window,
                     int32_t surface_id,
                     int32_t top_margin)
    : arc_app_window_(arc_app_window),
      surface_id_(surface_id),
      top_margin_(top_margin) {
  other_windows_observer_.Add(arc_app_window_);

  host_->set_owned_by_client();
  auto* const widget = views::Widget::GetWidgetForNativeWindow(arc_app_window_);
  DCHECK(widget);
  widget->GetContentsView()->AddChildView(host_.get());
}

CustomTab::~CustomTab() = default;

void CustomTab::Attach(gfx::NativeView view) {
  DCHECK(view);
  DCHECK(!GetHostView());
  host_->Attach(view);
  aura::Window* const container = host_->GetNativeViewContainer();
  container->SetEventTargeter(std::make_unique<aura::WindowTargeter>());
  other_windows_observer_.Add(container);
  EnsureWindowOrders();
  UpdateSurfaceIfNecessary();
}

gfx::NativeView CustomTab::GetHostView() {
  return host_->native_view();
}

void CustomTab::OnWindowHierarchyChanged(const HierarchyChangeParams& params) {
  if ((params.receiver == arc_app_window_) &&
      exo::Surface::AsSurface(params.target) && params.new_parent)
    UpdateSurfaceIfNecessary();
}

void CustomTab::OnWindowBoundsChanged(aura::Window* window,
                                      const gfx::Rect& old_bounds,
                                      const gfx::Rect& new_bounds,
                                      ui::PropertyChangeReason reason) {
  if (surface_window_observer_.IsObserving(window) &&
      old_bounds.size() != new_bounds.size())
    OnSurfaceBoundsMaybeChanged(window);
}

void CustomTab::OnWindowPropertyChanged(aura::Window* window,
                                        const void* key,
                                        intptr_t old) {
  if (surfaces_observer_.IsObserving(window) && key == exo::kClientSurfaceIdKey)
    UpdateSurfaceIfNecessary();
}

void CustomTab::OnWindowStackingChanged(aura::Window* window) {
  if (window == host_->GetNativeViewContainer() &&
      !weak_ptr_factory_.HasWeakPtrs()) {
    // Reordering should happen asynchronously -- some entity (like
    // views::WindowReorderer) changes the window orders, and then ensures layer
    // orders later. Changing order here synchronously leads to inconsistent
    // window/layer ordering and causes weird graphical effects.
    // TODO(hashimoto): fix the views ordering and remove this handling.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&CustomTab::EnsureWindowOrders,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void CustomTab::OnWindowDestroying(aura::Window* window) {
  if (surfaces_observer_.IsObserving(window))
    surfaces_observer_.Remove(window);
  if (surface_window_observer_.IsObserving(window))
    surface_window_observer_.Remove(window);
  if (other_windows_observer_.IsObserving(window))
    other_windows_observer_.Remove(window);
}

void CustomTab::OnSurfaceBoundsMaybeChanged(aura::Window* surface_window) {
  DCHECK(surface_window);
  gfx::Point origin(0, top_margin_);
  gfx::Point bottom_right(surface_window->bounds().width(),
                          surface_window->bounds().height());
  ConvertPointFromWindow(surface_window, &origin);
  ConvertPointFromWindow(surface_window, &bottom_right);
  host_->SetBounds(origin.x(), origin.y(), bottom_right.x() - origin.x(),
                   bottom_right.y() - origin.y());
}

void CustomTab::EnsureWindowOrders() {
  aura::Window* const container = host_->GetNativeViewContainer();
  if (container)
    container->parent()->StackChildAtTop(container);
}

void CustomTab::ConvertPointFromWindow(aura::Window* window,
                                       gfx::Point* point) {
  views::Widget* const widget = host_->GetWidget();
  aura::Window::ConvertPointToTarget(window, widget->GetNativeWindow(), point);
  views::View::ConvertPointFromWidget(widget->GetContentsView(), point);
}

void CustomTab::UpdateSurfaceIfNecessary() {
  std::vector<exo::Surface*> surfaces;
  EnumerateSurfaces(arc_app_window_, &surfaces);

  // Try to find the surface.
  const auto it = std::find_if(surfaces.cbegin(), surfaces.cend(),
                               [id = surface_id_](const auto* surface) {
                                 return surface->GetClientSurfaceId() == id;
                               });
  if (it == surfaces.cend()) {
    for (auto* surface : surfaces) {
      if (!surface->GetClientSurfaceId() &&
          !surfaces_observer_.IsObserving(surface->window()))
        surfaces_observer_.Add(surface->window());
    }
  } else {
    surfaces_observer_.RemoveAll();

    auto* const window = (*it)->window();
    if (!surface_window_observer_.IsObserving(window)) {
      surface_window_observer_.RemoveAll();
      surface_window_observer_.Add(window);
      OnSurfaceBoundsMaybeChanged(window);
    }
  }
}

}  // namespace arc
