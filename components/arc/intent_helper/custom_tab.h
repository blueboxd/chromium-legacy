// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_CUSTOM_TAB_H_
#define COMPONENTS_ARC_INTENT_HELPER_CUSTOM_TAB_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/arc/arc_export.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/native/native_view_host.h"

namespace arc {

// CustomTab is responsible to embed an ARC++ custom tab.
class ARC_EXPORT CustomTab : public aura::WindowObserver {
 public:
  CustomTab(aura::Window* arc_app_window,
            int32_t surface_id,
            int32_t top_margin);
  CustomTab(const CustomTab&) = delete;
  CustomTab& operator=(const CustomTab&) = delete;
  ~CustomTab() override;

  void Attach(gfx::NativeView view);

  // Returns the view against which a view or dialog is positioned and parented
  // in an CustomTab.
  gfx::NativeView GetHostView();

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

 private:
  // Updates |host_|'s bounds to deal with changes in the bounds of the
  // associated |surface_window|.
  void OnSurfaceBoundsMaybeChanged(aura::Window* surface_window);

  // Ensures the window/layer orders for the NativeViewHost.
  void EnsureWindowOrders();

  // Converts the point from the given window to this view.
  void ConvertPointFromWindow(aura::Window* window, gfx::Point* point);

  // Looks for the surface with |surface_id_|, and handles resultant changes.
  void UpdateSurfaceIfNecessary();

  std::unique_ptr<views::NativeViewHost> host_ =
      std::make_unique<views::NativeViewHost>();
  aura::Window* const arc_app_window_;
  const int32_t surface_id_, top_margin_;
  ScopedObserver<aura::Window, aura::WindowObserver> surfaces_observer_{this};
  ScopedObserver<aura::Window, aura::WindowObserver> surface_window_observer_{
      this};
  ScopedObserver<aura::Window, aura::WindowObserver> other_windows_observer_{
      this};
  base::WeakPtrFactory<CustomTab> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_CUSTOM_TAB_H_
