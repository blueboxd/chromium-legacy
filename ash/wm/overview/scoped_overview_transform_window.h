// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_OVERVIEW_TRANSFORM_WINDOW_H_
#define ASH_WM_OVERVIEW_SCOPED_OVERVIEW_TRANSFORM_WINDOW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_types.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "ui/aura/client/transient_window_client_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace aura {
class ScopedWindowEventTargetingBlocker;
class Window;
}  // namespace aura

namespace ash {
class OverviewItem;
class ScopedOverviewAnimationSettings;
class ScopedOverviewHideWindows;

// Manages a window, and its transient children, in the overview mode. This
// class allows transforming the windows with a helper to determine the best
// fit in certain bounds. The window's state is restored when this object is
// destroyed.
class ASH_EXPORT ScopedOverviewTransformWindow
    : public aura::client::TransientWindowClientObserver {
 public:
  using ScopedAnimationSettings =
      std::vector<std::unique_ptr<ScopedOverviewAnimationSettings>>;

  // Calculates and returns an optimal scale ratio. This is only taking into
  // account |size.height()| as the width can vary.
  static float GetItemScale(const gfx::SizeF& source,
                            const gfx::SizeF& target,
                            int top_view_inset,
                            int title_height);

  static OverviewGridWindowFillMode GetWindowDimensionsType(
      const gfx::Size& size);

  ScopedOverviewTransformWindow(OverviewItem* overview_item,
                                aura::Window* window);
  ~ScopedOverviewTransformWindow() override;

  // Starts an animation sequence which will use animation settings specified by
  // |animation_type|. The |animation_settings| container is populated with
  // scoped entities and the container should be destroyed at the end of the
  // animation sequence.
  //
  // Example:
  //  ScopedOverviewTransformWindow overview_window(window);
  //  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  //  overview_window.BeginScopedAnimation(
  //      OVERVIEW_ANIMATION_RESTORE_WINDOW, &animation_settings);
  //  // Calls to SetTransform & SetOpacity will use the same animation settings
  //  // until animation_settings is destroyed.
  //  OverviewUtil::SetTransform(root_window, new_transform);
  //  overview_window.SetOpacity(1);
  void BeginScopedAnimation(OverviewAnimationType animation_type,
                            ScopedAnimationSettings* animation_settings);

  // Returns true if this overview window contains the |target|.
  bool Contains(const aura::Window* target) const;

  // Returns transformed bounds of the overview window. See
  // OverviewUtil::GetTransformedBounds for more details.
  gfx::RectF GetTransformedBounds() const;

  // Returns the kTopViewInset property of |window_| unless there are transient
  // ancestors, in which case returns 0.
  int GetTopInset() const;

  // Restores and animates the managed window to its non overview mode state.
  // If |reset_transform| equals false, the window's transform will not be reset
  // to identity transform when exiting the overview mode. See
  // OverviewItem::RestoreWindow() for details why we need this.
  void RestoreWindow(bool reset_transform);

  // Prepares for overview mode by doing any necessary actions before entering.
  void PrepareForOverview();

  // Sets the opacity of the managed windows.
  void SetOpacity(float opacity);

  // Apply clipping on the managed windows. If |size| is empty, then restore
  // |overview_clip_rect_|.
  void SetClipping(const gfx::SizeF& size);

  // Returns |rect| having been shrunk to fit within |bounds| (preserving the
  // aspect ratio). Takes into account a window header that is |top_view_inset|
  // tall in the original window getting replaced by a window caption that is
  // |title_height| tall in the transformed window.
  gfx::RectF ShrinkRectToFitPreservingAspectRatio(const gfx::RectF& rect,
                                                  const gfx::RectF& bounds,
                                                  int top_view_inset,
                                                  int title_height);

  // Returns the window used to show the content in overview mode.
  // For minimized window this will be a window that hosts mirrored layers.
  aura::Window* GetOverviewWindow() const;

  // Closes the transient root of the window managed by |this|.
  void Close();

  bool IsMinimized() const;

  // Ensures that a window is visible by setting its opacity to 1.
  void EnsureVisible();

  // Called via OverviewItem from OverviewGrid when |window_|'s bounds
  // change. Must be called before PositionWindows in OverviewGrid.
  void UpdateWindowDimensionsType();

  // Updates the rounded corners and clipping on the window. Note that the
  // rounded corners can be hidden with |show| set to false, but the clipping
  // stays for the duration of overview once applied.
  void UpdateRoundedCornersAndClip(bool show);

  // Clip the top portion of the window that normally contains the caption (if
  // any).
  void ClipHeaderIfNeeded(bool animate);

  // aura::client::TransientWindowClientObserver:
  void OnTransientChildWindowAdded(aura::Window* parent,
                                   aura::Window* transient_child) override;
  void OnTransientChildWindowRemoved(aura::Window* parent,
                                     aura::Window* transient_child) override;

  aura::Window* window() const { return window_; }

  OverviewGridWindowFillMode type() const { return type_; }

 private:
  friend class OverviewHighlightControllerTest;
  friend class OverviewSessionTest;
  class LayerCachingAndFilteringObserver;

  // Closes the window managed by |this|.
  void CloseWidget();

  // Makes Close() execute synchronously when used in tests.
  static void SetImmediateCloseForTests();

  // A weak pointer to the overview item that owns |this|. Guaranteed to be not
  // null for the lifetime of |this|.
  OverviewItem* overview_item_;

  // A weak pointer to the real window in the overview.
  aura::Window* window_;

  // The original opacity of the window before entering overview mode.
  float original_opacity_;

  // Specifies how the window is laid out in the grid.
  OverviewGridWindowFillMode type_ = OverviewGridWindowFillMode::kNormal;

  // The observers associated with the layers we requested caching render
  // surface and trilinear filtering. The requests will be removed in dtor if
  // the layer has not been destroyed.
  std::vector<std::unique_ptr<LayerCachingAndFilteringObserver>>
      cached_and_filtered_layer_observers_;

  // For the duration of this object |window_| and its transient childrens'
  // event targeting policy will be sent to NONE. Store the originals so we can
  // change it back when destroying |this|.
  base::flat_map<aura::Window*,
                 std::unique_ptr<aura::ScopedWindowEventTargetingBlocker>>
      event_targeting_blocker_map_;

  // The original clipping on the layer of the window before entering overview
  // mode.
  gfx::Rect original_clip_rect_;

  // The clippng on the layer of |window_| after entering overview mode.
  // Additional clipping may be added, and when that additional clipping is
  // removed, we should go back to this clipping.
  gfx::Rect overview_clip_rect_;

  // True if a window is clipped to match splitview bounds. If true, the
  // splitview clipping overrides any top view inset clipping there may be.
  bool has_aspect_ratio_clipping_ = false;

  std::unique_ptr<ScopedOverviewHideWindows> hidden_transient_children_;

  base::WeakPtrFactory<ScopedOverviewTransformWindow> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ScopedOverviewTransformWindow);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_OVERVIEW_TRANSFORM_WINDOW_H_
