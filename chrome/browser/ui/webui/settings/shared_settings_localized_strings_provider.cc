// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "ui/base/webui/web_ui_util.h"

namespace settings {
namespace {

void AddA11yStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"a11yPageTitle", IDS_SETTINGS_ACCESSIBILITY},
    {"a11yWebStore", IDS_SETTINGS_ACCESSIBILITY_WEB_STORE},
    {"moreFeaturesLinkDescription",
     IDS_SETTINGS_MORE_FEATURES_LINK_DESCRIPTION},
    {"accessibleImageLabelsTitle", IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_TITLE},
    {"accessibleImageLabelsSubtitle",
     IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_SUBTITLE},
    {"settingsSliderRoleDescription",
     IDS_SETTINGS_SLIDER_MIN_MAX_ARIA_ROLE_DESCRIPTION},
#if defined(OS_CHROMEOS)
    {"manageAccessibilityFeatures",
     IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
#endif  // defined(OS_CHROMEOS)
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean(
      "showExperimentalA11yLabels",
      base::FeatureList::IsEnabled(features::kExperimentalAccessibilityLabels));

  html_source->AddBoolean(
      "enableCaptionSettings",
      base::FeatureList::IsEnabled(features::kCaptionSettings));

  html_source->AddBoolean("enableLiveCaption",
                          base::FeatureList::IsEnabled(media::kLiveCaption));
}

}  // namespace

void AddSharedLocalizedStrings(content::WebUIDataSource* html_source,
                               Profile* profile,
                               content::WebContents* web_contents) {
  AddA11yStrings(html_source);
}

void AddSharedA11yStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"captionsTitle", IDS_SETTINGS_CAPTIONS},
      {"captionsSettings", IDS_SETTINGS_CAPTIONS_SETTINGS},
      {"captionsPreview", IDS_SETTINGS_CAPTIONS_PREVIEW},
      {"captionsTextSize", IDS_SETTINGS_CAPTIONS_TEXT_SIZE},
      {"captionsTextFont", IDS_SETTINGS_CAPTIONS_TEXT_FONT},
      {"captionsTextColor", IDS_SETTINGS_CAPTIONS_TEXT_COLOR},
      {"captionsTextOpacity", IDS_SETTINGS_CAPTIONS_TEXT_OPACITY},
      {"captionsBackgroundOpacity", IDS_SETTINGS_CAPTIONS_BACKGROUND_OPACITY},
      {"captionsOpacityOpaque", IDS_SETTINGS_CAPTIONS_OPACITY_OPAQUE},
      {"captionsOpacitySemiTransparent",
       IDS_SETTINGS_CAPTIONS_OPACITY_SEMI_TRANSPARENT},
      {"captionsOpacityTransparent", IDS_SETTINGS_CAPTIONS_OPACITY_TRANSPARENT},
      {"captionsTextShadow", IDS_SETTINGS_CAPTIONS_TEXT_SHADOW},
      {"captionsTextShadowNone", IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_NONE},
      {"captionsTextShadowRaised", IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_RAISED},
      {"captionsTextShadowDepressed",
       IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_DEPRESSED},
      {"captionsTextShadowUniform", IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_UNIFORM},
      {"captionsTextShadowDropShadow",
       IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_DROP_SHADOW},
      {"captionsBackgroundColor", IDS_SETTINGS_CAPTIONS_BACKGROUND_COLOR},
      {"captionsColorBlack", IDS_SETTINGS_CAPTIONS_COLOR_BLACK},
      {"captionsColorWhite", IDS_SETTINGS_CAPTIONS_COLOR_WHITE},
      {"captionsColorRed", IDS_SETTINGS_CAPTIONS_COLOR_RED},
      {"captionsColorGreen", IDS_SETTINGS_CAPTIONS_COLOR_GREEN},
      {"captionsColorBlue", IDS_SETTINGS_CAPTIONS_COLOR_BLUE},
      {"captionsColorYellow", IDS_SETTINGS_CAPTIONS_COLOR_YELLOW},
      {"captionsColorCyan", IDS_SETTINGS_CAPTIONS_COLOR_CYAN},
      {"captionsColorMagenta", IDS_SETTINGS_CAPTIONS_COLOR_MAGENTA},
      {"captionsDefaultSetting", IDS_SETTINGS_CAPTIONS_DEFAULT_SETTING},
      {"captionsEnableLiveCaption", IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_CAPTION},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

}  // namespace settings
