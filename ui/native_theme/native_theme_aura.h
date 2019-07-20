// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_AURA_H_
#define UI_NATIVE_THEME_NATIVE_THEME_AURA_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "ui/native_theme/native_theme_base.h"

namespace ui {

// Aura implementation of native theme support.
class NATIVE_THEME_EXPORT NativeThemeAura : public NativeThemeBase {
 protected:
  friend class NativeTheme;
  friend class NativeThemeAuraTest;
  friend class base::NoDestructor<NativeThemeAura>;

  explicit NativeThemeAura(bool use_overlay_scrollbars);
  ~NativeThemeAura() override;

  static NativeThemeAura* instance();
  static NativeThemeAura* web_instance();

  // NativeThemeBase:
  SkColor GetSystemColor(ColorId color_id) const override;
  void PaintMenuPopupBackground(
      cc::PaintCanvas* canvas,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& menu_background) const override;
  void PaintMenuItemBackground(
      cc::PaintCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuItemExtraParams& menu_item) const override;
  void PaintArrowButton(cc::PaintCanvas* gc,
                        const gfx::Rect& rect,
                        Part direction,
                        State state) const override;
  void PaintScrollbarTrack(cc::PaintCanvas* canvas,
                           Part part,
                           State state,
                           const ScrollbarTrackExtraParams& extra_params,
                           const gfx::Rect& rect) const override;
  void PaintScrollbarThumb(cc::PaintCanvas* canvas,
                           Part part,
                           State state,
                           const gfx::Rect& rect,
                           ScrollbarOverlayColorTheme theme) const override;
  void PaintScrollbarCorner(cc::PaintCanvas* canvas,
                            State state,
                            const gfx::Rect& rect) const override;
  void PaintCheckbox(cc::PaintCanvas* canvas,
                     State state,
                     const gfx::Rect& rect,
                     const ButtonExtraParams& button) const override;
  void PaintRadio(cc::PaintCanvas* canvas,
                  State state,
                  const gfx::Rect& rect,
                  const ButtonExtraParams& button) const override;
  void PaintTextField(cc::PaintCanvas* canvas,
                      State state,
                      const gfx::Rect& rect,
                      const TextFieldExtraParams& text) const override;
  void PaintButton(cc::PaintCanvas* canvas,
                   State state,
                   const gfx::Rect& rect,
                   const ButtonExtraParams& button) const override;
  void PaintSliderTrack(cc::PaintCanvas* canvas,
                        State state,
                        const gfx::Rect& rect,
                        const SliderExtraParams& slider) const override;
  void PaintSliderThumb(cc::PaintCanvas* canvas,
                        State state,
                        const gfx::Rect& rect,
                        const SliderExtraParams& slider) const override;
  void PaintMenuList(cc::PaintCanvas* canvas,
                     State state,
                     const gfx::Rect& rect,
                     const MenuListExtraParams& menu_list) const override;
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size GetNinePatchCanvasSize(Part part) const override;
  gfx::Rect GetNinePatchAperture(Part part) const override;

 private:
  // Paint the common parts of the checkboxes and radio buttons.
  // borderRadius specifies how rounded the corners should be.
  SkRect PaintCheckboxRadioCommon(cc::PaintCanvas* canvas,
                                  State state,
                                  const gfx::Rect& rect,
                                  const SkScalar borderRadius) const;

  bool use_overlay_scrollbars_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeAura);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_AURA_H_
