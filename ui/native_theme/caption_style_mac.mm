// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <AppKit/AppKit.h>
#include <MediaAccessibility/MediaAccessibility.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/caption_style.h"

namespace ui {
#if 0
namespace {

constexpr auto kUserDomain = kMACaptionAppearanceDomainUser;

// Each of these functions returns a MediaAccessibility (MA) default value as a
// CSS specifier string whose format is appropriate to the property. Some notes
// about these:
//   1) The behavior of MA properties (applies only as a default vs mandatory)
//      is ignored deliberately at the moment; there's no good way to express
//      this tri-state behavior in CSS and CaptionStyle doesn't support it on
//      other platforms. All the MA properties are treated as defaults here.
//   2) The MA "window" concept is not implemented at all; it doesn't exist in
//      CaptionStyle or in the Blink-side WebVTT implementation.
//   3) If a function is confused about how to map the system value to a Blink
//      WebVTT attribute (or a CSS specifier), it's always safe to return an
//      empty string, which will cause Blink to fall back to its default
//      behavior for that attribute.
//   4) The only useful domain to retrieve attributes from is kUserDomain; the
//      system domain's values never change.

std::string GetMAForegroundColorAndOpacityAsCSSColor() {
  base::ScopedCFTypeRef<CGColorRef> cg_color(
      MACaptionAppearanceCopyForegroundColor(kUserDomain, nullptr));
  float opacity = MACaptionAppearanceGetForegroundOpacity(kUserDomain, nullptr);

  SkColor rgba_color =
      SkColorSetA(skia::CGColorRefToSkColor(cg_color.get()), 0xff * opacity);
  return color_utils::SkColorToRgbaString(rgba_color);
}

std::string GetMABackgroundColorAndOpacityAsCSSColor() {
  base::ScopedCFTypeRef<CGColorRef> cg_color(
      MACaptionAppearanceCopyBackgroundColor(kUserDomain, nullptr));
  float opacity = MACaptionAppearanceGetBackgroundOpacity(kUserDomain, nullptr);

  SkColor rgba_color =
      SkColorSetA(skia::CGColorRefToSkColor(cg_color.get()), 0xff * opacity);
  return color_utils::SkColorToRgbaString(rgba_color);
}

// The MA text scale is a float between 0.0 and 2.0; this function converts it
// to a CSS string specifying a percentage to scale the text by.
std::string GetMATextScaleAsCSSPercent() {
  int percent =
      100 * MACaptionAppearanceGetRelativeCharacterSize(kUserDomain, nullptr);
  return base::StringPrintf("%d%%", percent);
}

std::string GetMATextEdgeStyleAsCSSShadow() {
  MACaptionAppearanceTextEdgeStyle style =
      MACaptionAppearanceGetTextEdgeStyle(kUserDomain, nullptr);

  // The MACaptionAppearanceTextEdgeStyle -> CSS shadow specs in this function
  // were found by experimentation and eyeballing - there's no known
  // documentation of how the MA shadow constants are supposed to look.
  switch (style) {
    case kMACaptionAppearanceTextEdgeStyleUndefined:
    case kMACaptionAppearanceTextEdgeStyleNone:
      // It's not clear when Undefined can be returned - here it's simply
      // treated as a synonym for None.
      return "";
    case kMACaptionAppearanceTextEdgeStyleRaised:
      return "-2px -2px 4px rgba(0, 0, 0, 0.5)";
    case kMACaptionAppearanceTextEdgeStyleDepressed:
      return "2px 2px 4px rgba(0, 0, 0, 0.5)";
    case kMACaptionAppearanceTextEdgeStyleUniform:
      // This system style looks like a 2px black stroke drawn around the edge
      // of the text. This isn't doable using CSS shadows, but drawing black
      // shadows around all the edges of the text produces a reasonable
      // imitation.
      return "-1px 0px 0px black, 0px -1px 0px black, "
             " 1px 0px 0px black, 0px  1px 0px black";
    case kMACaptionAppearanceTextEdgeStyleDropShadow:
      // Compose a pair of shadows to create the "drop" effect - a
      // semi-transparent shadow around the text, then a darker shadow below and
      // to the right of it.
      return "0px 0px 2px rgba(0, 0, 0, 0.5), 2px 2px 2px black";
  }
}

// Thankfully, CSS font descriptors can simply use font names understood by the
// platform, so that's what this function does. It only returns attributes for
// the default font. The system allows configuring default font variants for
// each font face to be used in WebVTT captions, which is not implemented here.
void GetMAFontAsCSSFontSpecifiers(std::string* font_family,
                                  std::string* font_variant) {
  base::ScopedCFTypeRef<CTFontDescriptorRef> ct_font_desc(
      MACaptionAppearanceCopyFontDescriptorForStyle(
          kUserDomain, nullptr, kMACaptionAppearanceFontStyleDefault));

  base::ScopedCFTypeRef<CFStringRef> ct_font_family_name(
      base::mac::CFCast<CFStringRef>(CTFontDescriptorCopyAttribute(
          ct_font_desc, kCTFontFamilyNameAttribute)));
  if (ct_font_family_name)
    *font_family = base::SysCFStringRefToUTF8(ct_font_family_name);

  base::ScopedCFTypeRef<CFStringRef> ct_font_face_name(
      base::mac::CFCast<CFStringRef>(
          CTFontDescriptorCopyAttribute(ct_font_desc, kCTFontNameAttribute)));
  if (ct_font_face_name)
    *font_variant = base::SysCFStringRefToUTF8(ct_font_face_name);
}

}  // namespace
#endif
// static
base::Optional<CaptionStyle> CaptionStyle::FromSystemSettings() {
  CaptionStyle style;

//  style.text_color = GetMAForegroundColorAndOpacityAsCSSColor();
//  style.background_color = GetMABackgroundColorAndOpacityAsCSSColor();
//  style.text_size = GetMATextScaleAsCSSPercent();
//  style.text_shadow = GetMATextEdgeStyleAsCSSShadow();
//
//  GetMAFontAsCSSFontSpecifiers(&style.font_family, &style.font_variant);

  return style;
}

}  // namespace ui
