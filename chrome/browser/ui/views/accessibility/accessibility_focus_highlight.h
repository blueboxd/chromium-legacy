// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_FOCUS_HIGHLIGHT_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_FOCUS_HIGHLIGHT_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/rect.h"

class BrowserView;

namespace ui {
class Compositor;
class Layer;
}  // namespace ui

// AccessibilityFocusHighlight manages an always-on-top layer used to
// highlight the focused UI element for accessibility.
class AccessibilityFocusHighlight : public ui::LayerDelegate,
                                    public ui::CompositorAnimationObserver,
                                    public content::NotificationObserver {
 public:
  explicit AccessibilityFocusHighlight(BrowserView* browser_view);
  ~AccessibilityFocusHighlight() override;

  // Disallow copy and assign.
  AccessibilityFocusHighlight(const AccessibilityFocusHighlight&) = delete;
  AccessibilityFocusHighlight& operator=(const AccessibilityFocusHighlight&) =
      delete;

 private:
  FRIEND_TEST_ALL_PREFIXES(AccessibilityFocusHighlightBrowserTest,
                           DrawsHighlight);
  // For testing.
  static void SetNoFadeForTesting();
  static void SkipActivationCheckForTesting();
  static SkColor GetHighlightColorForTesting();

  // Create the layer if needed, and update its bounds to match |bounds_|.
  void CreateOrUpdateLayer();

  // Get rid of the layer and stop animation.
  void RemoveLayer();

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // CompositorAnimationObserver overrides:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // The layer, if visible.
  std::unique_ptr<ui::Layer> layer_;

  // The compositor associated with this layer.
  ui::Compositor* compositor_ = nullptr;

  // The bounding rectangle of the focused object, in the coordinate system
  // of our owner BrowserView's layer.
  gfx::Rect bounds_;

  // Owns this.
  BrowserView* browser_view_;

  // The time the layer was created and started fading in.
  base::TimeTicks layer_created_time_;

  // The most recent time the layer was updated because focus moved.
  base::TimeTicks focus_last_changed_time_;

  // The current scale factor between DIPs and pixels.
  float device_scale_factor_;

  // The color used for the highlight.
  static SkColor color_;

  // The amount of time it should take for the highlight to fade in.
  static base::TimeDelta fade_in_time_;

  // The amount of time it should take for the highlight to fade out.
  static base::TimeDelta fade_out_time_;

  // If set, draws the highlight even if the widget is not active.
  static bool skip_activation_check_for_testing_;

  // For observing focus notifications.
  content::NotificationRegistrar notification_registrar_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_FOCUS_HIGHLIGHT_H_
