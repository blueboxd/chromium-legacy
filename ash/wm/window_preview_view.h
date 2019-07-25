// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PREVIEW_VIEW_H_
#define ASH_WM_WINDOW_PREVIEW_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/window_mirror_view.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "ui/aura/client/transient_window_client_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/view.h"

namespace ash {

// A view that mirrors the client area of a window and all its transient
// descendants.
class ASH_EXPORT WindowPreviewView
    : public views::View,
      public aura::client::TransientWindowClientObserver,
      public aura::WindowObserver {
 public:
  WindowPreviewView(aura::Window* window, bool trilinear_filtering_on_init);
  ~WindowPreviewView() override;

  // Recreate the preview views for the window and all its transient
  // descendants.
  void RecreatePreviews();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

  // aura::client::TransientWindowClientObserver:
  void OnTransientChildWindowAdded(aura::Window* parent,
                                   aura::Window* transient_child) override;
  void OnTransientChildWindowRemoved(aura::Window* parent,
                                     aura::Window* transient_child) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  aura::Window* window() { return window_; }

 private:
  friend class WindowPreviewViewTestApi;

  void AddWindow(aura::Window* window);
  void RemoveWindow(aura::Window* window);

  // Get the smallest rectangle that contains the bounds of all the windows in
  // |mirror_views_|.
  gfx::RectF GetUnionRect() const;

  aura::Window* window_;
  bool trilinear_filtering_on_init_;

  base::flat_map<aura::Window*, WindowMirrorView*> mirror_views_;

  DISALLOW_COPY_AND_ASSIGN(WindowPreviewView);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_PREVIEW_VIEW_H_
