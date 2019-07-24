// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ASH_COLOR_PROVIDER_H_
#define ASH_STYLE_ASH_COLOR_PROVIDER_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// The color provider for system UI. It provides colors for Shield layer, Base
// layer and +1 layer. Shield layer is a combination of color, opacity and blur
// which may change depending on the context, it is usually a fullscreen layer.
// e.g, PowerButtoneMenuScreenView for power button menu. Base layer is the
// bottom layer of any UI displayed on top of all other UIs. e.g, the ShelfView
// that contains all the shelf items. Controls layer is where components such as
// icons and inkdrops lay on, it may also indicate the state of an interactive
// element (active/inactive states). The color of an element in system UI will
// be the combination of the colors of the three layers.
class AshColorProvider {
 public:
  // The color mode of system UI. It is controlled by the feature flag
  // "--ash-color-mode" currently.
  enum class AshColorMode {
    // This is the color mode of current system UI, which is a combination of
    // dark and light mode. e.g, shelf and system tray are dark while many other
    // elements like notification are light.
    kDefault = 0,
    // The text is black while the background is white or light.
    kLight,
    // The text is light color while the background is black or dark grey.
    kDark
  };

  // Types of Shield layer.
  enum class ShieldLayerType {
    kAlpha20,  // opacity of the layer is 20%
    kAlpha40,  // opacity of the layer is 40%
    kAlpha60,  // opacity of the layer is 60%
  };

  // Types of Base layer.
  enum class BaseLayerType {
    // Base layer is transparent with blur.
    kTransparentWithBlur = 0,

    // Base layer is transparent without blur.
    kTransparentWithoutBlur,

    // Base layer is opaque.
    kOpaque,
  };

  // Types of Controls layer.
  enum class ControlsLayerType {
    kHairlineBorder,
    kSeparator,
    kActiveControlBackground,
    kInactiveControlBackground,
    kFocusRing,
  };

  // Attributes of ripple, includes the base color, opacity of inkdrop and
  // highlight.
  struct RippleAttributes {
    RippleAttributes(SkColor color,
                     float opacity_of_inkdrop,
                     float opacity_of_highlight)
        : base_color(color),
          inkdrop_opacity(opacity_of_inkdrop),
          highlight_opacity(opacity_of_highlight) {}
    const SkColor base_color;
    const float inkdrop_opacity;
    const float highlight_opacity;
  };

  AshColorProvider();
  ~AshColorProvider();

  // Gets the color of corresponding layer for specific type.
  SkColor GetShieldLayerColor(ShieldLayerType type) const;
  SkColor GetBaseLayerColor(BaseLayerType type) const;
  SkColor GetControlsLayerColor(ControlsLayerType type) const;

  // Gets the attributes of ripple on |bg_color|. |bg_color| is the background
  // color of the UI element that wants to show inkdrop.
  RippleAttributes GetRippleAttributes(SkColor bg_color) const;

  AshColorMode color_mode() const { return color_mode_; }

 private:
  // Selects from |light_color| and |dark_color| based on |color_mode_|.
  SkColor SelectColorOnMode(SkColor light_color, SkColor dark_color) const;

  // Current color mode of system UI.
  AshColorMode color_mode_ = AshColorMode::kDefault;

  DISALLOW_COPY_AND_ASSIGN(AshColorProvider);
};

}  // namespace ash

#endif  // ASH_STYLE_ASH_COLOR_PROVIDER_H_
