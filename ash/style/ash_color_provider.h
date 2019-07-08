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
// that contains all the shelf items. +1 layer is where components such as icons
// and inkdrops lay on, it may also indicate the state of an interactive
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

  // TODO(minch): To consider whether we should split this into two enums like
  // ShieldOpacity and ShieldBlur.
  // Types of Shield layer.
  enum class ShieldLayerType {
    // The layer has blur and opacity is 20%.
    kAlpha20WithBlur,

    // The layer has blur and opacity is 40%.
    kAlpha40WithBlur,

    // The layer has blur and opacity is 60%.
    kAlpha60WithBlur,

    // The layer doesn't have blur and opacity is 20%.
    kAlpha20WithoutBlur,

    // The layer doesn't have blur and opacity is 40%.
    kAlpha40WithoutBlur,

    // The layer doesn't have blur and opacity is 60%.
    kAlpha60WithoutBlur,
  };

  // TODO(minch): Revise the names of these layers after their usage becomes
  // clearer or discuss with PM/UX to suggest better names.
  // Types of Base layer.
  enum class BaseLayerType {
    // Base layer is transparent with blur.
    kTransparentWithBlur = 0,

    // Base layer is transparent without blur.
    kTransparentWithoutBlur,

    // Base layer is opaque.
    kOpaque,
  };

  // TODO(minch): Discuss with UX to check whether the grouping here is
  // reasonable. e.g, `kSeparator` looks very odd here.
  // Types of +1 layer.
  enum class PlusOneLayerType {
    // The +1 layer has a border with kHairLinePx thickness.
    kHairLine = 0,

    // The +1 layer is a separator, could be vertical or horizontal.
    kSeparator,

    // The state of the +1 layer is inactive or active. They will be served as a
    // base for interactive layers, where ripple will be used as tap/touch
    // feedback.
    kInActive,
    kActive,
  };

  AshColorProvider();
  ~AshColorProvider();

  // Gets the color of corresponding layer for specific type.
  SkColor GetShieldLayerColor(ShieldLayerType type) const;
  SkColor GetBaseLayerColor(BaseLayerType type) const;
  SkColor GetPlusOneLayerColor(PlusOneLayerType type) const;

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
