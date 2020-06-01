// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_VIEW_H_
#define ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_VIEW_H_

#include <memory>

#include "ash/public/cpp/arc_custom_tab.h"
#include "base/scoped_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view.h"

namespace ash {

// A view-based implementation of ArcCustomTab which works in the classic
// environment.
class ArcCustomTabView : public ArcCustomTab,
                         public views::View,
                         public aura::WindowObserver {
 public:
  ArcCustomTabView(aura::Window* arc_app_window,
                   int32_t surface_id,
                   int32_t top_margin);
  ArcCustomTabView(const ArcCustomTabView&) = delete;
  ArcCustomTabView& operator=(const ArcCustomTabView&) = delete;
  ~ArcCustomTabView() override;

 private:
  // ArcCustomTab:
  void Attach(gfx::NativeView view) override;
  gfx::NativeView GetHostView() override;

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Updates |host_|'s bounds to deal with changes in the bounds of the
  // associated |surface_window|.
  void OnSurfaceBoundsMaybeChanged(aura::Window* surface_window);

  // Ensures the window/layer orders for the NativeViewHost.
  void EnsureWindowOrders();

  // Converts the point from the given window to this view.
  void ConvertPointFromWindow(aura::Window* window, gfx::Point* point);

  // Looks for the surface with |surface_id_|, and handles resultant changes.
  void UpdateSurfaceIfNecessary();

  views::NativeViewHost* const host_ =
      AddChildView(std::make_unique<views::NativeViewHost>());
  aura::Window* const arc_app_window_;
  const int32_t surface_id_, top_margin_;
  ScopedObserver<aura::Window, aura::WindowObserver> surfaces_observer_{this};
  ScopedObserver<aura::Window, aura::WindowObserver> surface_window_observer_{
      this};
  ScopedObserver<aura::Window, aura::WindowObserver> other_windows_observer_{
      this};
  base::WeakPtrFactory<ArcCustomTabView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_VIEW_H_
