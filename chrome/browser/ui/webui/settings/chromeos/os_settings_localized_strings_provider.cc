// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_localized_strings_provider.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/number_formatting.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_utils.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_dialog_localized_strings_provider.h"
#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"
#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_shares_localized_strings_provider.h"
#include "chrome/browser/ui/webui/management_ui.h"
#include "chrome/browser/ui/webui/policy_indicator_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/internet_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/services/local_search_service/public/mojom/types.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/services/assistant/public/features.h"
#include "chromeos/services/multidevice_setup/public/cpp/url_provider.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/bluetooth/strings/grit/bluetooth_strings.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/display/display_features.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/touch_device_manager.h"

namespace chromeos {
namespace settings {
namespace {

std::vector<local_search_service::mojom::DataPtr> ConceptVectorToDataPtrVector(
    const std::vector<SearchConcept>& tags_group) {
  std::vector<local_search_service::mojom::DataPtr> data_list;

  for (const auto& concept : tags_group) {
    std::vector<base::string16> search_tags;

    // Add the canonical tag.
    search_tags.push_back(
        l10n_util::GetStringUTF16(concept.canonical_message_id));

    // Add all alternate tags.
    for (size_t i = 0; i < SearchConcept::kMaxAltTagsPerConcept; ++i) {
      int curr_alt_tag = concept.alt_tag_ids[i];
      if (curr_alt_tag == SearchConcept::kAltTagEnd)
        break;
      search_tags.push_back(l10n_util::GetStringUTF16(curr_alt_tag));
    }

    // Note: A stringified version of the canonical tag message ID is used as
    // the identifier for this search data.
    data_list.push_back(local_search_service::mojom::Data::New(
        base::NumberToString(concept.canonical_message_id), search_tags));
  }

  return data_list;
}

// Generates a Google Help URL which includes a "board type" parameter. Some
// help pages need to be adjusted depending on the type of CrOS device that is
// accessing the page.
base::string16 GetHelpUrlWithBoard(const std::string& original_url) {
  return base::ASCIIToUTF16(original_url +
                            "&b=" + base::SysInfo::GetLsbReleaseBoard());
}

bool IsDeviceManaged() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

bool IsProfileManaged(Profile* profile) {
  return profile->GetProfilePolicyConnector()->IsManaged();
}

void AddCommonStrings(content::WebUIDataSource* html_source, Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"add", IDS_ADD},
      {"advancedPageTitle", IDS_SETTINGS_ADVANCED},
      {"back", IDS_ACCNAME_BACK},
      {"basicPageTitle", IDS_SETTINGS_BASIC},
      {"cancel", IDS_CANCEL},
      {"clear", IDS_SETTINGS_CLEAR},
      {"close", IDS_CLOSE},
      {"confirm", IDS_CONFIRM},
      {"continue", IDS_SETTINGS_CONTINUE},
      {"controlledByExtension", IDS_SETTINGS_CONTROLLED_BY_EXTENSION},
      {"custom", IDS_SETTINGS_CUSTOM},
      {"delete", IDS_SETTINGS_DELETE},
      {"deviceOff", IDS_SETTINGS_DEVICE_OFF},
      {"deviceOn", IDS_SETTINGS_DEVICE_ON},
      {"disable", IDS_DISABLE},
      {"done", IDS_DONE},
      {"edit", IDS_SETTINGS_EDIT},
      {"extensionsLinkTooltip", IDS_SETTINGS_MENU_EXTENSIONS_LINK_TOOLTIP},
      {"learnMore", IDS_LEARN_MORE},
      {"menu", IDS_MENU},
      {"menuButtonLabel", IDS_SETTINGS_MENU_BUTTON_LABEL},
      {"moreActions", IDS_SETTINGS_MORE_ACTIONS},
      {"ok", IDS_OK},
      {"restart", IDS_SETTINGS_RESTART},
      {"save", IDS_SAVE},
      {"searchResultBubbleText", IDS_SEARCH_RESULT_BUBBLE_TEXT},
      {"searchResultsBubbleText", IDS_SEARCH_RESULTS_BUBBLE_TEXT},
      {"settings", IDS_SETTINGS_SETTINGS},
      {"settingsAltPageTitle", IDS_SETTINGS_ALT_PAGE_TITLE},
      {"subpageArrowRoleDescription", IDS_SETTINGS_SUBPAGE_BUTTON},
      {"notValidWebAddress", IDS_SETTINGS_NOT_VALID_WEB_ADDRESS},
      {"notValidWebAddressForContentType",
       IDS_SETTINGS_NOT_VALID_WEB_ADDRESS_FOR_CONTENT_TYPE},

      // Common font related strings shown in a11y and appearance sections.
      {"quickBrownFox", IDS_SETTINGS_QUICK_BROWN_FOX},
      {"verySmall", IDS_SETTINGS_VERY_SMALL_FONT},
      {"small", IDS_SETTINGS_SMALL_FONT},
      {"medium", IDS_SETTINGS_MEDIUM_FONT},
      {"large", IDS_SETTINGS_LARGE_FONT},
      {"veryLarge", IDS_SETTINGS_VERY_LARGE_FONT},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean(
      "isGuest",
      user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
          user_manager::UserManager::Get()->IsLoggedInAsPublicAccount());

  html_source->AddBoolean("isSupervised", profile->IsSupervised());
}

void AddA11yStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"a11yPageTitle", IDS_SETTINGS_ACCESSIBILITY},
      {"a11yWebStore", IDS_SETTINGS_ACCESSIBILITY_WEB_STORE},
      {"moreFeaturesLinkDescription",
       IDS_SETTINGS_MORE_FEATURES_LINK_DESCRIPTION},
      {"accessibleImageLabelsTitle",
       IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_TITLE},
      {"accessibleImageLabelsSubtitle",
       IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_SUBTITLE},
      {"settingsSliderRoleDescription",
       IDS_SETTINGS_SLIDER_MIN_MAX_ARIA_ROLE_DESCRIPTION},
      {"manageAccessibilityFeatures",
       IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
      {"optionsInMenuLabel", IDS_SETTINGS_OPTIONS_IN_MENU_LABEL},
      {"largeMouseCursorLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_LABEL},
      {"largeMouseCursorSizeLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_LABEL},
      {"largeMouseCursorSizeDefaultLabel",
       IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_DEFAULT_LABEL},
      {"largeMouseCursorSizeLargeLabel",
       IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_LARGE_LABEL},
      {"highContrastLabel", IDS_SETTINGS_HIGH_CONTRAST_LABEL},
      {"stickyKeysLabel", IDS_SETTINGS_STICKY_KEYS_LABEL},
      {"chromeVoxLabel", IDS_SETTINGS_CHROMEVOX_LABEL},
      {"chromeVoxOptionsLabel", IDS_SETTINGS_CHROMEVOX_OPTIONS_LABEL},
      {"screenMagnifierLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_LABEL},
      {"screenMagnifierZoomLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_LABEL},
      {"dockedMagnifierLabel", IDS_SETTINGS_DOCKED_MAGNIFIER_LABEL},
      {"dockedMagnifierZoomLabel", IDS_SETTINGS_DOCKED_MAGNIFIER_ZOOM_LABEL},
      {"screenMagnifierZoom2x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_2_X},
      {"screenMagnifierZoom4x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_4_X},
      {"screenMagnifierZoom6x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_6_X},
      {"screenMagnifierZoom8x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_8_X},
      {"screenMagnifierZoom10x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_10_X},
      {"screenMagnifierZoom12x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_12_X},
      {"screenMagnifierZoom14x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_14_X},
      {"screenMagnifierZoom16x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_16_X},
      {"screenMagnifierZoom18x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_18_X},
      {"screenMagnifierZoom20x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_20_X},
      {"tapDraggingLabel", IDS_SETTINGS_TAP_DRAGGING_LABEL},
      {"clickOnStopLabel", IDS_SETTINGS_CLICK_ON_STOP_LABEL},
      {"delayBeforeClickLabel", IDS_SETTINGS_DELAY_BEFORE_CLICK_LABEL},
      {"delayBeforeClickExtremelyShort",
       IDS_SETTINGS_DELAY_BEFORE_CLICK_EXTREMELY_SHORT},
      {"delayBeforeClickVeryShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_SHORT},
      {"delayBeforeClickShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_SHORT},
      {"delayBeforeClickLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_LONG},
      {"delayBeforeClickVeryLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_LONG},
      {"autoclickRevertToLeftClick",
       IDS_SETTINGS_AUTOCLICK_REVERT_TO_LEFT_CLICK},
      {"autoclickStabilizeCursorPosition",
       IDS_SETTINGS_AUTOCLICK_STABILIZE_CURSOR_POSITION},
      {"autoclickMovementThresholdLabel",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_LABEL},
      {"autoclickMovementThresholdExtraSmall",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_EXTRA_SMALL},
      {"autoclickMovementThresholdSmall",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_SMALL},
      {"autoclickMovementThresholdDefault",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_DEFAULT},
      {"autoclickMovementThresholdLarge",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_LARGE},
      {"autoclickMovementThresholdExtraLarge",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_EXTRA_LARGE},
      {"dictationDescription",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_DESCRIPTION},
      {"dictationLabel", IDS_SETTINGS_ACCESSIBILITY_DICTATION_LABEL},
      {"onScreenKeyboardLabel", IDS_SETTINGS_ON_SCREEN_KEYBOARD_LABEL},
      {"monoAudioLabel", IDS_SETTINGS_MONO_AUDIO_LABEL},
      {"startupSoundLabel", IDS_SETTINGS_STARTUP_SOUND_LABEL},
      {"a11yExplanation", IDS_SETTINGS_ACCESSIBILITY_EXPLANATION},
      {"caretHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_CARET_HIGHLIGHT_DESCRIPTION},
      {"cursorHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_CURSOR_HIGHLIGHT_DESCRIPTION},
      {"focusHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_FOCUS_HIGHLIGHT_DESCRIPTION},
      {"selectToSpeakTitle", IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_TITLE},
      {"selectToSpeakDisabledDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DISABLED_DESCRIPTION},
      {"selectToSpeakDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION},
      {"selectToSpeakDescriptionWithoutKeyboard",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION_WITHOUT_KEYBOARD},
      {"selectToSpeakOptionsLabel",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_OPTIONS_LABEL},
      {"switchAccessLabel",
       IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_DESCRIPTION},
      {"switchAccessOptionsLabel",
       IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_OPTIONS_LABEL},
      {"manageSwitchAccessSettings",
       IDS_SETTINGS_MANAGE_SWITCH_ACCESS_SETTINGS},
      {"switchAssignmentHeading", IDS_SETTINGS_SWITCH_ASSIGNMENT_HEADING},
      {"switchAssignOptionNone", IDS_SETTINGS_SWITCH_ASSIGN_OPTION_NONE},
      {"switchAssignOptionSpace", IDS_SETTINGS_SWITCH_ASSIGN_OPTION_SPACE},
      {"switchAssignOptionEnter", IDS_SETTINGS_SWITCH_ASSIGN_OPTION_ENTER},
      {"assignSelectSwitchLabel", IDS_SETTINGS_ASSIGN_SELECT_SWITCH_LABEL},
      {"assignNextSwitchLabel", IDS_SETTINGS_ASSIGN_NEXT_SWITCH_LABEL},
      {"assignPreviousSwitchLabel", IDS_SETTINGS_ASSIGN_PREVIOUS_SWITCH_LABEL},
      {"switchAccessAutoScanHeading",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_HEADING},
      {"switchAccessAutoScanLabel", IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_LABEL},
      {"switchAccessAutoScanSpeedLabel",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_SPEED_LABEL},
      {"switchAccessAutoScanKeyboardSpeedLabel",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_KEYBOARD_SPEED_LABEL},
      {"durationInSeconds", IDS_SETTINGS_DURATION_IN_SECONDS},
      {"manageAccessibilityFeatures",
       IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
      {"textToSpeechHeading",
       IDS_SETTINGS_ACCESSIBILITY_TEXT_TO_SPEECH_HEADING},
      {"displayHeading", IDS_SETTINGS_ACCESSIBILITY_DISPLAY_HEADING},
      {"displaySettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_TITLE},
      {"displaySettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_DESCRIPTION},
      {"appearanceSettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_TITLE},
      {"appearanceSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_DESCRIPTION},
      {"keyboardAndTextInputHeading",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_AND_TEXT_INPUT_HEADING},
      {"keyboardSettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_TITLE},
      {"keyboardSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_DESCRIPTION},
      {"mouseAndTouchpadHeading",
       IDS_SETTINGS_ACCESSIBILITY_MOUSE_AND_TOUCHPAD_HEADING},
      {"mouseSettingsTitle", IDS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_TITLE},
      {"mouseSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_DESCRIPTION},
      {"audioAndCaptionsHeading",
       IDS_SETTINGS_ACCESSIBILITY_AUDIO_AND_CAPTIONS_HEADING},
      {"additionalFeaturesTitle",
       IDS_SETTINGS_ACCESSIBILITY_ADDITIONAL_FEATURES_TITLE},
      {"manageTtsSettings", IDS_SETTINGS_MANAGE_TTS_SETTINGS},
      {"ttsSettingsLinkDescription", IDS_SETTINGS_TTS_LINK_DESCRIPTION},
      {"textToSpeechVoices", IDS_SETTINGS_TEXT_TO_SPEECH_VOICES},
      {"textToSpeechNoVoicesMessage",
       IDS_SETTINGS_TEXT_TO_SPEECH_NO_VOICES_MESSAGE},
      {"textToSpeechMoreLanguages", IDS_SETTINGS_TEXT_TO_SPEECH_MORE_LANGUAGES},
      {"textToSpeechProperties", IDS_SETTINGS_TEXT_TO_SPEECH_PROPERTIES},
      {"textToSpeechRate", IDS_SETTINGS_TEXT_TO_SPEECH_RATE},
      {"textToSpeechRateMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_RATE_MINIMUM_LABEL},
      {"textToSpeechRateMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_RATE_MAXIMUM_LABEL},
      {"textToSpeechPitch", IDS_SETTINGS_TEXT_TO_SPEECH_PITCH},
      {"textToSpeechPitchMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PITCH_MINIMUM_LABEL},
      {"textToSpeechPitchMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PITCH_MAXIMUM_LABEL},
      {"textToSpeechVolume", IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME},
      {"textToSpeechVolumeMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME_MINIMUM_LABEL},
      {"textToSpeechVolumeMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME_MAXIMUM_LABEL},
      {"percentage", IDS_SETTINGS_PERCENTAGE},
      {"defaultPercentage", IDS_SETTINGS_DEFAULT_PERCENTAGE},
      {"textToSpeechPreviewHeading",
       IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_HEADING},
      {"textToSpeechPreviewInputLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_INPUT_LABEL},
      {"textToSpeechPreviewInput", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_INPUT},
      {"textToSpeechPreviewVoice", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_VOICE},
      {"textToSpeechPreviewPlay", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_PLAY},
      {"textToSpeechEngines", IDS_SETTINGS_TEXT_TO_SPEECH_ENGINES},
      {"tabletModeShelfNavigationButtonsSettingLabel",
       IDS_SETTINGS_A11Y_TABLET_MODE_SHELF_BUTTONS_LABEL},
      {"tabletModeShelfNavigationButtonsSettingDescription",
       IDS_SETTINGS_A11Y_TABLET_MODE_SHELF_BUTTONS_DESCRIPTION},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString("accountManagerLearnMoreUrl",
                         chrome::kAccountManagerLearnMoreURL);
  html_source->AddString("a11yLearnMoreUrl",
                         chrome::kChromeAccessibilityHelpURL);

  base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  html_source->AddBoolean(
      "showExperimentalAccessibilitySwitchAccess",
      cmd.HasSwitch(::switches::kEnableExperimentalAccessibilitySwitchAccess));

  html_source->AddBoolean(
      "showExperimentalAccessibilitySwitchAccessImprovedTextInput",
      cmd.HasSwitch(
          ::switches::kEnableExperimentalAccessibilitySwitchAccessText));

  html_source->AddBoolean("showExperimentalA11yLabels",
                          base::FeatureList::IsEnabled(
                              ::features::kExperimentalAccessibilityLabels));

  html_source->AddBoolean(
      "showTabletModeShelfNavigationButtonsSettings",
      ash::features::IsHideShelfControlsInTabletModeEnabled());

  html_source->AddString("tabletModeShelfNavigationButtonsLearnMoreUrl",
                         chrome::kTabletModeGesturesLearnMoreURL);

  html_source->AddBoolean("enableLiveCaption",
                          base::FeatureList::IsEnabled(media::kLiveCaption));

  ::settings::AddCaptionSubpageStrings(html_source);
}

void AddSmartInputsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"smartInputsTitle", IDS_SETTINGS_SMART_INPUTS_TITLE},
      {"personalInfoSuggestionTitle",
       IDS_SETTINGS_SMART_INPUTS_PERSONAL_INFO_TITLE},
      {"personalInfoSuggestionDescription",
       IDS_SETTINGS_SMART_INPUTS_PERSONAL_INFO_DESCRIPTION},
      {"showPersonalInfoSuggestion",
       IDS_SETTINGS_SMART_INPUTS_SHOW_PERSONAL_INFO},
      {"managePersonalInfo", IDS_SETTINGS_SMART_INPUTS_MANAGE_PERSONAL_INFO},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  // Assistive personal info is allowed when the feature flag is enabled and the
  // user is not in guest mode.
  html_source->AddBoolean(
      "allowAssistivePersonalInfo",
      base::FeatureList::IsEnabled(features::kAssistPersonalInfo) &&
          !(user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
            user_manager::UserManager::Get()->IsLoggedInAsPublicAccount()));
}

void AddLanguagesStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"orderLanguagesInstructions",
       IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_ORDERING_INSTRUCTIONS},
      {"osLanguagesPageTitle", IDS_OS_SETTINGS_LANGUAGES_AND_INPUT_PAGE_TITLE},
      {"osLanguagesListTitle", IDS_OS_SETTINGS_LANGUAGES_LIST_TITLE},
      {"inputMethodsListTitle",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_LIST_TITLE},
      {"inputMethodEnabled", IDS_SETTINGS_LANGUAGES_INPUT_METHOD_ENABLED},
      {"inputMethodsExpandA11yLabel",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_EXPAND_ACCESSIBILITY_LABEL},
      {"inputMethodsManagedbyPolicy",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_MANAGED_BY_POLICY},
      {"manageInputMethods", IDS_SETTINGS_LANGUAGES_INPUT_METHODS_MANAGE},
      {"manageInputMethodsPageTitle",
       IDS_SETTINGS_LANGUAGES_MANAGE_INPUT_METHODS_TITLE},
      {"showImeMenu", IDS_SETTINGS_LANGUAGES_SHOW_IME_MENU},
      {"displayLanguageRestart",
       IDS_SETTINGS_LANGUAGES_RESTART_TO_DISPLAY_LANGUAGE},
      {"moveDown", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_DOWN},
      {"displayInThisLanguage",
       IDS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE},
      {"searchLanguages", IDS_SETTINGS_LANGUAGE_SEARCH},
      {"addLanguagesDialogTitle",
       IDS_SETTINGS_LANGUAGES_MANAGE_LANGUAGES_TITLE},
      {"moveToTop", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_TO_TOP},
      {"isDisplayedInThisLanguage",
       IDS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE},
      {"removeLanguage", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_REMOVE},
      {"addLanguages", IDS_SETTINGS_LANGUAGES_LANGUAGES_ADD},
      {"moveUp", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_UP},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
  AddSmartInputsStrings(html_source);

  html_source->AddString(
      "languagesLearnMoreURL",
      base::ASCIIToUTF16(chrome::kLanguageSettingsLearnMoreUrl));
}

void AddPersonalizationStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"ambientModeTitle", IDS_OS_SETTINGS_AMBIENT_MODE_TITLE},
      {"ambientModeEnabled", IDS_OS_SETTINGS_AMBIENT_MODE_ENABLED},
      {"ambientModeDisabled", IDS_OS_SETTINGS_AMBIENT_MODE_DISABLED},
      {"ambientModeOn", IDS_OS_SETTINGS_AMBIENT_MODE_ON},
      {"ambientModeOff", IDS_OS_SETTINGS_AMBIENT_MODE_OFF},
      {"ambientModeTopicSourceTitle",
       IDS_OS_SETTINGS_AMBIENT_MODE_TOPIC_SOURCE_TITLE},
      {"ambientModeTopicSourceGooglePhotos",
       IDS_OS_SETTINGS_AMBIENT_MODE_TOPIC_SOURCE_GOOGLE_PHOTOS},
      {"ambientModeTopicSourceArtGallery",
       IDS_OS_SETTINGS_AMBIENT_MODE_TOPIC_SOURCE_ART_GALLERY},
      {"changePictureTitle", IDS_OS_SETTINGS_CHANGE_PICTURE_TITLE},
      {"openWallpaperApp", IDS_OS_SETTINGS_OPEN_WALLPAPER_APP},
      {"personalizationPageTitle", IDS_OS_SETTINGS_PERSONALIZATION},
      {"setWallpaper", IDS_OS_SETTINGS_SET_WALLPAPER},
      {"takePhoto", IDS_SETTINGS_CHANGE_PICTURE_TAKE_PHOTO},
      {"captureVideo", IDS_SETTINGS_CHANGE_PICTURE_CAPTURE_VIDEO},
      {"discardPhoto", IDS_SETTINGS_CHANGE_PICTURE_DISCARD_PHOTO},
      {"previewAltText", IDS_SETTINGS_CHANGE_PICTURE_PREVIEW_ALT},
      {"switchModeToVideo", IDS_SETTINGS_CHANGE_PICTURE_SWITCH_MODE_TO_VIDEO},
      {"profilePhoto", IDS_SETTINGS_CHANGE_PICTURE_PROFILE_PHOTO},
      {"changePicturePageDescription", IDS_SETTINGS_CHANGE_PICTURE_DIALOG_TEXT},
      {"switchModeToCamera", IDS_SETTINGS_CHANGE_PICTURE_SWITCH_MODE_TO_CAMERA},
      {"chooseFile", IDS_SETTINGS_CHANGE_PICTURE_CHOOSE_FILE},
      {"oldPhoto", IDS_SETTINGS_CHANGE_PICTURE_OLD_PHOTO},
      {"oldVideo", IDS_SETTINGS_CHANGE_PICTURE_OLD_VIDEO},
      {"authorCreditText", IDS_SETTINGS_CHANGE_PICTURE_AUTHOR_CREDIT_TEXT},
      {"photoCaptureAccessibleText",
       IDS_SETTINGS_PHOTO_CAPTURE_ACCESSIBLE_TEXT},
      {"photoDiscardAccessibleText",
       IDS_SETTINGS_PHOTO_DISCARD_ACCESSIBLE_TEXT},
      {"photoModeAccessibleText", IDS_SETTINGS_PHOTO_MODE_ACCESSIBLE_TEXT},
      {"videoModeAccessibleText", IDS_SETTINGS_VIDEO_MODE_ACCESSIBLE_TEXT},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean(
      "changePictureVideoModeEnabled",
      base::FeatureList::IsEnabled(::features::kChangePictureVideoMode));
  html_source->AddBoolean("isAmbientModeEnabled",
                          chromeos::features::IsAmbientModeEnabled());
}

void AddFingerprintListStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"lockScreenAddFingerprint",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_ADD_FINGERPRINT_BUTTON},
      {"lockScreenRegisteredFingerprints",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_REGISTERED_FINGERPRINTS_LABEL},
      {"lockScreenFingerprintWarning",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_LESS_SECURE},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

void AddSetupPinDialogStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"configurePinChoosePinTitle",
       IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_CHOOSE_PIN_TITLE},
      {"configurePinConfirmPinTitle",
       IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_CONFIRM_PIN_TITLE},
      {"configurePinMismatched", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_MISMATCHED},
      {"configurePinTooShort", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_TOO_SHORT},
      {"configurePinTooLong", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_TOO_LONG},
      {"configurePinWeakPin", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_WEAK_PIN},
      {"pinKeyboardPlaceholderPin", IDS_PIN_KEYBOARD_HINT_TEXT_PIN},
      {"pinKeyboardPlaceholderPinPassword",
       IDS_PIN_KEYBOARD_HINT_TEXT_PIN_PASSWORD},
      {"pinKeyboardDeleteAccessibleName",
       IDS_PIN_KEYBOARD_DELETE_ACCESSIBLE_NAME},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  // Format numbers to be used on the pin keyboard.
  for (int j = 0; j <= 9; j++) {
    html_source->AddString("pinKeyboard" + base::NumberToString(j),
                           base::FormatNumber(int64_t{j}));
  }
}

void AddSetupFingerprintDialogStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"configureFingerprintTitle", IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_TITLE},
      {"configureFingerprintAddAnotherButton",
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_ADD_ANOTHER_BUTTON},
      {"configureFingerprintInstructionReadyStep",
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_READY},
      {"configureFingerprintLiftFinger",
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_LIFT_FINGER},
      {"configureFingerprintTryAgain",
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_TRY_AGAIN},
      {"configureFingerprintImmobile",
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_FINGER_IMMOBILE},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

void AddFingerprintStrings(content::WebUIDataSource* html_source) {
  int instruction_id, aria_label_id;
  using FingerprintLocation = chromeos::quick_unlock::FingerprintLocation;
  switch (chromeos::quick_unlock::GetFingerprintLocation()) {
    case FingerprintLocation::TABLET_POWER_BUTTON:
      instruction_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_POWER_BUTTON;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_POWER_BUTTON_ARIA_LABEL;
      break;
    case FingerprintLocation::KEYBOARD_BOTTOM_LEFT:
      instruction_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD_BOTTOM_LEFT_ARIA_LABEL;
      break;
    case FingerprintLocation::KEYBOARD_BOTTOM_RIGHT:
      instruction_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD_BOTTOM_RIGHT_ARIA_LABEL;
      break;
    case FingerprintLocation::KEYBOARD_TOP_RIGHT:
      instruction_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD_TOP_RIGHT_ARIA_LABEL;
      break;
  }
  html_source->AddLocalizedString(
      "configureFingerprintInstructionLocateScannerStep", instruction_id);
  html_source->AddLocalizedString("configureFingerprintScannerStepAriaLabel",
                                  aria_label_id);
}

void AddAccountManagerPageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"accountManagerDescription", IDS_SETTINGS_ACCOUNT_MANAGER_DESCRIPTION},
      {"accountManagerChildDescription",
       IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_DESCRIPTION},
      {"accountListHeader", IDS_SETTINGS_ACCOUNT_MANAGER_LIST_HEADER},
      {"accountManagerPrimaryAccountTooltip",
       IDS_SETTINGS_ACCOUNT_MANAGER_PRIMARY_ACCOUNT_TOOLTIP},
      {"accountManagerEducationAccountLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_EDUCATION_ACCOUNT},
      {"removeAccountLabel", IDS_SETTINGS_ACCOUNT_MANAGER_REMOVE_ACCOUNT_LABEL},
      {"addAccountLabel", IDS_SETTINGS_ACCOUNT_MANAGER_ADD_ACCOUNT_LABEL},
      {"addSchoolAccountLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_ADD_SCHOOL_ACCOUNT_LABEL},
      {"accountManagerSecondaryAccountsDisabledText",
       IDS_SETTINGS_ACCOUNT_MANAGER_SECONDARY_ACCOUNTS_DISABLED_TEXT},
      {"accountManagerSecondaryAccountsDisabledChildText",
       IDS_SETTINGS_ACCOUNT_MANAGER_SECONDARY_ACCOUNTS_DISABLED_CHILD_TEXT},
      {"accountManagerSignedOutAccountName",
       IDS_SETTINGS_ACCOUNT_MANAGER_SIGNED_OUT_ACCOUNT_PLACEHOLDER},
      {"accountManagerUnmigratedAccountName",
       IDS_SETTINGS_ACCOUNT_MANAGER_UNMIGRATED_ACCOUNT_PLACEHOLDER},
      {"accountManagerMigrationLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_MIGRATION_LABEL},
      {"accountManagerReauthenticationLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_REAUTHENTICATION_LABEL},
      {"accountManagerMigrationTooltip",
       IDS_SETTINGS_ACCOUNT_MANAGER_MIGRATION_TOOLTIP},
      {"accountManagerReauthenticationTooltip",
       IDS_SETTINGS_ACCOUNT_MANAGER_REAUTHENTICATION_TOOLTIP},
      {"accountManagerMoreActionsTooltip",
       IDS_SETTINGS_ACCOUNT_MANAGER_MORE_ACTIONS_TOOLTIP},
      {"accountManagerManagedLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_MANAGEMENT_STATUS_MANAGED_ACCOUNT},
      {"accountManagerUnmanagedLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_MANAGEMENT_STATUS_UNMANAGED_ACCOUNT},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

void AddSyncControlsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"syncEverythingCheckboxLabel",
       IDS_SETTINGS_SYNC_EVERYTHING_CHECKBOX_LABEL},
      {"wallpaperCheckboxLabel", IDS_OS_SETTINGS_WALLPAPER_CHECKBOX_LABEL},
      {"osSyncTurnOff", IDS_OS_SETTINGS_SYNC_TURN_OFF},
      {"osSyncSettingsCheckboxLabel",
       IDS_OS_SETTINGS_SYNC_SETTINGS_CHECKBOX_LABEL},
      {"wifiConfigurationsCheckboxLabel",
       IDS_SETTINGS_WIFI_CONFIGURATIONS_CHECKBOX_LABEL},
      {"osSyncAppsCheckboxLabel", IDS_OS_SETTINGS_SYNC_APPS_CHECKBOX_LABEL},
      {"osSyncTurnOn", IDS_OS_SETTINGS_SYNC_TURN_ON},
      {"osSyncFeatureLabel", IDS_OS_SETTINGS_SYNC_FEATURE_LABEL},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString(
      "browserSettingsSyncSetupUrl",
      base::StrCat({chrome::kChromeUISettingsURL, chrome::kSyncSetupSubPage}));
}

void AddCrostiniStrings(content::WebUIDataSource* html_source,
                        Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"crostiniPageTitle", IDS_SETTINGS_CROSTINI_TITLE},
      {"crostiniPageLabel", IDS_SETTINGS_CROSTINI_LABEL},
      {"crostiniEnable", IDS_SETTINGS_TURN_ON},
      {"crostiniSharedPaths", IDS_SETTINGS_CROSTINI_SHARED_PATHS},
      {"crostiniSharedPathsListHeading",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_LIST_HEADING},
      {"crostiniSharedPathsInstructionsAdd",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_INSTRUCTIONS_ADD},
      {"crostiniSharedPathsInstructionsRemove",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_INSTRUCTIONS_REMOVE},
      {"crostiniSharedPathsRemoveSharing",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_REMOVE_SHARING},
      {"crostiniSharedPathsRemoveFailureDialogMessage",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_REMOVE_FAILURE_DIALOG_MESSAGE},
      {"crostiniSharedPathsRemoveFailureDialogTitle",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_REMOVE_FAILURE_DIALOG_TITLE},
      {"crostiniSharedPathsRemoveFailureTryAgain",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_REMOVE_FAILURE_TRY_AGAIN},
      {"crostiniSharedPathsListEmptyMessage",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_LIST_EMPTY_MESSAGE},
      {"crostiniExportImportTitle", IDS_SETTINGS_CROSTINI_EXPORT_IMPORT_TITLE},
      {"crostiniExport", IDS_SETTINGS_CROSTINI_EXPORT},
      {"crostiniExportLabel", IDS_SETTINGS_CROSTINI_EXPORT_LABEL},
      {"crostiniImport", IDS_SETTINGS_CROSTINI_IMPORT},
      {"crostiniImportLabel", IDS_SETTINGS_CROSTINI_IMPORT_LABEL},
      {"crostiniImportConfirmationDialogTitle",
       IDS_SETTINGS_CROSTINI_CONFIRM_IMPORT_DIALOG_WINDOW_TITLE},
      {"crostiniImportConfirmationDialogMessage",
       IDS_SETTINGS_CROSTINI_CONFIRM_IMPORT_DIALOG_WINDOW_MESSAGE},
      {"crostiniImportConfirmationDialogConfirmationButton",
       IDS_SETTINGS_CROSTINI_IMPORT},
      {"crostiniRemoveButton", IDS_SETTINGS_CROSTINI_REMOVE_BUTTON},
      {"crostiniSharedUsbDevicesLabel",
       IDS_SETTINGS_CROSTINI_SHARED_USB_DEVICES_LABEL},
      {"crostiniSharedUsbDevicesDescription",
       IDS_SETTINGS_CROSTINI_SHARED_USB_DEVICES_DESCRIPTION},
      {"crostiniSharedUsbDevicesExtraDescription",
       IDS_SETTINGS_CROSTINI_SHARED_USB_DEVICES_EXTRA_DESCRIPTION},
      {"crostiniSharedUsbDevicesListEmptyMessage",
       IDS_SETTINGS_CROSTINI_SHARED_USB_DEVICES_LIST_EMPTY_MESSAGE},
      {"crostiniArcAdbTitle", IDS_SETTINGS_CROSTINI_ARC_ADB_TITLE},
      {"crostiniArcAdbDescription", IDS_SETTINGS_CROSTINI_ARC_ADB_DESCRIPTION},
      {"crostiniArcAdbLabel", IDS_SETTINGS_CROSTINI_ARC_ADB_LABEL},
      {"crostiniArcAdbRestartButton",
       IDS_SETTINGS_CROSTINI_ARC_ADB_RESTART_BUTTON},
      {"crostiniArcAdbConfirmationTitleEnable",
       IDS_SETTINGS_CROSTINI_ARC_ADB_CONFIRMATION_TITLE_ENABLE},
      {"crostiniArcAdbConfirmationTitleDisable",
       IDS_SETTINGS_CROSTINI_ARC_ADB_CONFIRMATION_TITLE_DISABLE},
      {"crostiniContainerUpgrade",
       IDS_SETTINGS_CROSTINI_CONTAINER_UPGRADE_MESSAGE},
      {"crostiniContainerUpgradeSubtext",
       IDS_SETTINGS_CROSTINI_CONTAINER_UPGRADE_SUBTEXT},
      {"crostiniContainerUpgradeButton",
       IDS_SETTINGS_CROSTINI_CONTAINER_UPGRADE_BUTTON},
      {"crostiniPortForwarding", IDS_SETTINGS_CROSTINI_PORT_FORWARDING},
      {"crostiniPortForwardingDescription",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_DESCRIPTION},
      {"crostiniPortForwardingNoPorts",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_NO_PORTS},
      {"crostiniPortForwardingTableTitle",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_TABLE_TITLE},
      {"crostiniPortForwardingListPortNumber",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_LIST_PORT_NUMBER},
      {"crostiniPortForwardingListLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_LIST_LABEL},
      {"crostiniPortForwardingAddPortButton",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_BUTTON},
      {"crostiniPortForwardingAddPortButtonDescription",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_BUTTON_DESCRIPTION},
      {"crostiniPortForwardingAddPortDialogTitle",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_DIALOG_TITLE},
      {"crostiniPortForwardingAddPortDialogLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_DIALOG_LABEL},
      {"crostiniPortForwardingTCP", IDS_SETTINGS_CROSTINI_PORT_FORWARDING_TCP},
      {"crostiniPortForwardingUDP", IDS_SETTINGS_CROSTINI_PORT_FORWARDING_UDP},
      {"crostiniPortForwardingAddError",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_ERROR},
      {"crostiniDiskResizeTitle", IDS_SETTINGS_CROSTINI_DISK_RESIZE_TITLE},
      {"crostiniDiskResizeShowButton",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_SHOW_BUTTON},
      {"crostiniDiskResizeShowButtonAriaLabel",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_SHOW_BUTTON_ARIA_LABEL},
      {"crostiniDiskResizeLabel", IDS_SETTINGS_CROSTINI_DISK_RESIZE_LABEL},
      {"crostiniDiskResizeUnsupported",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_UNSUPPORTED},
      {"crostiniDiskResizeLoading", IDS_SETTINGS_CROSTINI_DISK_RESIZE_LOADING},
      {"crostiniDiskResizeError", IDS_SETTINGS_CROSTINI_DISK_RESIZE_ERROR},
      {"crostiniDiskResizeErrorRetry",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_ERROR_RETRY},
      {"crostiniDiskResizeCancel", IDS_SETTINGS_CROSTINI_DISK_RESIZE_CANCEL},
      {"crostiniDiskResizeGoButton",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_GO_BUTTON},
      {"crostiniDiskResizeInProgress",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_IN_PROGRESS},
      {"crostiniDiskResizeResizingError",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_RESIZING_ERROR},
      {"crostiniDiskResizeDone", IDS_SETTINGS_CROSTINI_DISK_RESIZE_DONE},
      {"crostiniMicTitle", IDS_SETTINGS_CROSTINI_MIC_TITLE},
      {"crostiniMicDialogTitle", IDS_SETTINGS_CROSTINI_MIC_DIALOG_TITLE},
      {"crostiniMicDialogLabel", IDS_SETTINGS_CROSTINI_MIC_DIALOG_LABEL},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
  html_source->AddString(
      "crostiniSubtext",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_SUBTEXT, ui::GetChromeOSDeviceName(),
          GetHelpUrlWithBoard(chrome::kLinuxAppsLearnMoreURL)));
  html_source->AddString(
      "crostiniArcAdbPowerwashRequiredSublabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_ARC_ADB_POWERWASH_REQUIRED_SUBLABEL,
          base::ASCIIToUTF16(chrome::kArcAdbSideloadingLearnMoreURL)));
  html_source->AddString("crostiniRemove", l10n_util::GetStringFUTF16(
                                               IDS_SETTINGS_CROSTINI_REMOVE,
                                               ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "crostiniArcAdbConfirmationMessageEnable",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_ARC_ADB_CONFIRMATION_MESSAGE_ENABLE,
          ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "crostiniArcAdbConfirmationMessageDisable",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_ARC_ADB_CONFIRMATION_MESSAGE_DISABLE,
          ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "crostiniSharedPathsInstructionsLocate",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_SHARED_PATHS_INSTRUCTIONS_LOCATE,
          base::ASCIIToUTF16(
              crostini::ContainerChromeOSBaseDirectory().value())));
  html_source->AddBoolean(
      "showCrostiniExportImport",
      crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile));
  html_source->AddBoolean("arcAdbSideloadingSupported",
                          base::FeatureList::IsEnabled(
                              chromeos::features::kArcAdbSideloadingFeature));
  html_source->AddBoolean("showCrostiniPortForwarding",
                          base::FeatureList::IsEnabled(
                              chromeos::features::kCrostiniPortForwarding));
  html_source->AddBoolean("isOwnerProfile",
                          chromeos::ProfileHelper::IsOwnerProfile(profile));
  html_source->AddBoolean("isEnterpriseManaged",
                          IsDeviceManaged() || IsProfileManaged(profile));
  html_source->AddBoolean(
      "canChangeAdbSideloading",
      crostini::CrostiniFeatures::Get()->CanChangeAdbSideloading(profile));
  html_source->AddBoolean("showCrostiniContainerUpgrade",
                          crostini::ShouldAllowContainerUpgrade(profile));
  html_source->AddBoolean(
      "showCrostiniDiskResize",
      base::FeatureList::IsEnabled(chromeos::features::kCrostiniDiskResizing));
  html_source->AddBoolean("showCrostiniMic",
                          base::FeatureList::IsEnabled(
                              chromeos::features::kCrostiniShowMicSetting));
}

void AddPluginVmStrings(content::WebUIDataSource* html_source,
                        Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"pluginVmPageTitle", IDS_SETTINGS_PLUGIN_VM_PAGE_TITLE},
      {"pluginVmPageLabel", IDS_SETTINGS_PLUGIN_VM_PAGE_LABEL},
      {"pluginVmPageSubtext", IDS_SETTINGS_PLUGIN_VM_PAGE_SUBTEXT},
      {"pluginVmPageEnable", IDS_SETTINGS_TURN_ON},
      {"pluginVmPrinterAccess", IDS_SETTINGS_PLUGIN_VM_PRINTER_ACCESS},
      {"pluginVmSharedPaths", IDS_SETTINGS_PLUGIN_VM_SHARED_PATHS},
      {"pluginVmSharedPathsListHeading",
       IDS_SETTINGS_PLUGIN_VM_SHARED_PATHS_LIST_HEADING},
      {"pluginVmSharedPathsInstructionsAdd",
       IDS_SETTINGS_PLUGIN_VM_SHARED_PATHS_INSTRUCTIONS_ADD},
      {"pluginVmSharedPathsInstructionsRemove",
       IDS_SETTINGS_PLUGIN_VM_SHARED_PATHS_INSTRUCTIONS_REMOVE},
      {"pluginVmSharedPathsRemoveSharing",
       IDS_SETTINGS_PLUGIN_VM_SHARED_PATHS_REMOVE_SHARING},
      {"pluginVmRemove", IDS_SETTINGS_PLUGIN_VM_REMOVE_LABEL},
      {"pluginVmRemoveButton", IDS_SETTINGS_PLUGIN_VM_REMOVE_BUTTON},
      {"pluginVmRemoveConfirmationDialogMessage",
       IDS_SETTINGS_PLUGIN_VM_CONFIRM_REMOVE_DIALOG_BODY},
      {"pluginVmCameraAccessTitle", IDS_SETTINGS_PLUGIN_VM_CAMERA_ACCESS_TITLE},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
  html_source->AddBoolean("showPluginVmCamera",
                          base::FeatureList::IsEnabled(
                              chromeos::features::kPluginVmShowCameraSetting));
}

void AddAndroidAppStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"androidAppsPageLabel", IDS_SETTINGS_ANDROID_APPS_LABEL},
      {"androidAppsEnable", IDS_SETTINGS_TURN_ON},
      {"androidAppsManageApps", IDS_SETTINGS_ANDROID_APPS_MANAGE_APPS},
      {"androidAppsRemove", IDS_SETTINGS_ANDROID_APPS_REMOVE},
      {"androidAppsRemoveButton", IDS_SETTINGS_ANDROID_APPS_REMOVE_BUTTON},
      {"androidAppsDisableDialogTitle",
       IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_TITLE},
      {"androidAppsDisableDialogMessage",
       IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_MESSAGE},
      {"androidAppsDisableDialogRemove",
       IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_REMOVE},
      {"androidAppsManageAppLinks", IDS_SETTINGS_ANDROID_APPS_MANAGE_APP_LINKS},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
  html_source->AddLocalizedString("androidAppsPageTitle",
                                  arc::IsPlayStoreAvailable()
                                      ? IDS_SETTINGS_ANDROID_APPS_TITLE
                                      : IDS_SETTINGS_ANDROID_SETTINGS_TITLE);
  html_source->AddString(
      "androidAppsSubtext",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ANDROID_APPS_SUBTEXT, ui::GetChromeOSDeviceName(),
          GetHelpUrlWithBoard(chrome::kAndroidAppsLearnMoreURL)));
}

void AddAppsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"appsPageTitle", IDS_SETTINGS_APPS_TITLE},
      {"appManagementTitle", IDS_SETTINGS_APPS_LINK_TEXT},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

void AddAppManagementStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"appManagementAppInstalledByPolicyLabel",
       IDS_APP_MANAGEMENT_POLICY_APP_POLICY_STRING},
      {"appManagementCameraPermissionLabel", IDS_APP_MANAGEMENT_CAMERA},
      {"appManagementContactsPermissionLabel", IDS_APP_MANAGEMENT_CONTACTS},
      {"appManagementLocationPermissionLabel", IDS_APP_MANAGEMENT_LOCATION},
      {"appManagementMicrophonePermissionLabel", IDS_APP_MANAGEMENT_MICROPHONE},
      {"appManagementMoreSettingsLabel", IDS_APP_MANAGEMENT_MORE_SETTINGS},
      {"appManagementNoAppsFound", IDS_APP_MANAGEMENT_NO_APPS_FOUND},
      {"appManagementNoPermissions",
       IDS_APPLICATION_INFO_APP_NO_PERMISSIONS_TEXT},
      {"appManagementNotificationsLabel", IDS_APP_MANAGEMENT_NOTIFICATIONS},
      {"appManagementPermissionsLabel", IDS_APP_MANAGEMENT_PERMISSIONS},
      {"appManagementPinToShelfLabel", IDS_APP_MANAGEMENT_PIN_TO_SHELF},
      {"appManagementSearchPrompt", IDS_APP_MANAGEMENT_SEARCH_PROMPT},
      {"appManagementStoragePermissionLabel", IDS_APP_MANAGEMENT_STORAGE},
      {"appManagementUninstallLabel", IDS_APP_MANAGEMENT_UNINSTALL_APP},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

void AddParentalControlStrings(content::WebUIDataSource* html_source,
                               Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"parentalControlsPageTitle", IDS_SETTINGS_PARENTAL_CONTROLS_PAGE_TITLE},
      {"parentalControlsPageSetUpLabel",
       IDS_SETTINGS_PARENTAL_CONTROLS_PAGE_SET_UP_LABEL},
      {"parentalControlsPageViewSettingsLabel",
       IDS_SETTINGS_PARENTAL_CONTROLS_PAGE_VIEW_SETTINGS_LABEL},
      {"parentalControlsPageConnectToInternetLabel",
       IDS_SETTINGS_PARENTAL_CONTROLS_PAGE_CONNECT_TO_INTERNET_LABEL},
      {"parentalControlsSetUpButtonLabel",
       IDS_SETTINGS_PARENTAL_CONTROLS_SET_UP_BUTTON_LABEL},
      {"parentalControlsSetUpButtonRole",
       IDS_SETTINGS_PARENTAL_CONTROLS_SET_UP_BUTTON_ROLE},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean(
      "isChild", user_manager::UserManager::Get()->IsLoggedInAsChildUser());

  if (user_manager::UserManager::Get()->IsLoggedInAsChildUser()) {
    SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    std::string custodian = supervised_user_service->GetCustodianName();
    std::string second_custodian =
        supervised_user_service->GetSecondCustodianName();

    base::string16 child_managed_tooltip;
    if (second_custodian.empty()) {
      child_managed_tooltip = l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_MANAGED_BY_ONE_PARENT_TOOLTIP,
          base::UTF8ToUTF16(custodian));
    } else {
      child_managed_tooltip = l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_MANAGED_BY_TWO_PARENTS_TOOLTIP,
          base::UTF8ToUTF16(custodian), base::UTF8ToUTF16(second_custodian));
    }
    html_source->AddString("accountManagerPrimaryAccountChildManagedTooltip",
                           child_managed_tooltip);
  }
}

void AddBluetoothStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"bluetoothConnected", IDS_SETTINGS_BLUETOOTH_CONNECTED},
      {"bluetoothConnectedWithBattery",
       IDS_SETTINGS_BLUETOOTH_CONNECTED_WITH_BATTERY},
      {"bluetoothConnecting", IDS_SETTINGS_BLUETOOTH_CONNECTING},
      {"bluetoothDeviceListPaired", IDS_SETTINGS_BLUETOOTH_DEVICE_LIST_PAIRED},
      {"bluetoothDeviceListUnpaired",
       IDS_SETTINGS_BLUETOOTH_DEVICE_LIST_UNPAIRED},
      {"bluetoothConnect", IDS_SETTINGS_BLUETOOTH_CONNECT},
      {"bluetoothDisconnect", IDS_SETTINGS_BLUETOOTH_DISCONNECT},
      {"bluetoothToggleA11yLabel",
       IDS_SETTINGS_BLUETOOTH_TOGGLE_ACCESSIBILITY_LABEL},
      {"bluetoothExpandA11yLabel",
       IDS_SETTINGS_BLUETOOTH_EXPAND_ACCESSIBILITY_LABEL},
      {"bluetoothNoDevices", IDS_SETTINGS_BLUETOOTH_NO_DEVICES},
      {"bluetoothNoDevicesFound", IDS_SETTINGS_BLUETOOTH_NO_DEVICES_FOUND},
      {"bluetoothNotConnected", IDS_SETTINGS_BLUETOOTH_NOT_CONNECTED},
      {"bluetoothPageTitle", IDS_SETTINGS_BLUETOOTH},
      {"bluetoothPairDevicePageTitle",
       IDS_SETTINGS_BLUETOOTH_PAIR_DEVICE_TITLE},
      {"bluetoothRemove", IDS_SETTINGS_BLUETOOTH_REMOVE},
      {"bluetoothPrimaryUserControlled",
       IDS_SETTINGS_BLUETOOTH_PRIMARY_USER_CONTROLLED},
      {"bluetoothDeviceType_computer",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_COMPUTER},
      {"bluetoothDeviceType_phone",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_PHONE},
      {"bluetoothDeviceType_modem",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_MODEM},
      {"bluetoothDeviceType_audio",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_AUDIO},
      {"bluetoothDeviceType_carAudio",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_CAR_AUDIO},
      {"bluetoothDeviceType_video",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_VIDEO},
      {"bluetoothDeviceType_peripheral",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_PERIPHERAL},
      {"bluetoothDeviceType_joystick",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_JOYSTICK},
      {"bluetoothDeviceType_gamepad",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_GAMEPAD},
      {"bluetoothDeviceType_keyboard",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_KEYBOARD},
      {"bluetoothDeviceType_mouse",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_MOUSE},
      {"bluetoothDeviceType_tablet",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_TABLET},
      {"bluetoothDeviceType_keyboardMouseCombo",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_KEYBOARD_MOUSE_COMBO},
      {"bluetoothDeviceType_unknown",
       IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_UNKNOWN},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
  chromeos::bluetooth_dialog::AddLocalizedStrings(html_source);
}

void AddChromeOSUserStrings(content::WebUIDataSource* html_source,
                            Profile* profile) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  const user_manager::User* primary_user = user_manager->GetPrimaryUser();
  std::string primary_user_email = primary_user->GetAccountId().GetUserEmail();
  html_source->AddString("primaryUserEmail", primary_user_email);
  html_source->AddString("browserSettingsBannerText",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_BROWSER_SETTINGS_BANNER,
                             base::ASCIIToUTF16(chrome::kChromeUISettingsURL)));
  html_source->AddBoolean("isActiveDirectoryUser",
                          user && user->IsActiveDirectoryUser());
  html_source->AddBoolean(
      "isSecondaryUser",
      user && user->GetAccountId() != primary_user->GetAccountId());
  html_source->AddString(
      "secondaryUserBannerText",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_SECONDARY_USER_BANNER,
                                 base::ASCIIToUTF16(primary_user_email)));

  if (!IsDeviceManaged() && !user_manager->IsCurrentUserOwner()) {
    html_source->AddString("ownerEmail",
                           user_manager->GetOwnerAccountId().GetUserEmail());
  }
}

void AddDevicePointersStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kPointersStrings[] = {
      {"mouseTitle", IDS_SETTINGS_MOUSE_TITLE},
      {"touchpadTitle", IDS_SETTINGS_TOUCHPAD_TITLE},
      {"mouseAndTouchpadTitle", IDS_SETTINGS_MOUSE_AND_TOUCHPAD_TITLE},
      {"touchpadTapToClickEnabledLabel",
       IDS_SETTINGS_TOUCHPAD_TAP_TO_CLICK_ENABLED_LABEL},
      {"touchpadSpeed", IDS_SETTINGS_TOUCHPAD_SPEED_LABEL},
      {"pointerSlow", IDS_SETTINGS_POINTER_SPEED_SLOW_LABEL},
      {"pointerFast", IDS_SETTINGS_POINTER_SPEED_FAST_LABEL},
      {"mouseScrollSpeed", IDS_SETTINGS_MOUSE_SCROLL_SPEED_LABEL},
      {"mouseSpeed", IDS_SETTINGS_MOUSE_SPEED_LABEL},
      {"mouseSwapButtons", IDS_SETTINGS_MOUSE_SWAP_BUTTONS_LABEL},
      {"mouseReverseScroll", IDS_SETTINGS_MOUSE_REVERSE_SCROLL_LABEL},
      {"mouseAccelerationLabel", IDS_SETTINGS_MOUSE_ACCELERATION_LABEL},
      {"mouseScrollAccelerationLabel",
       IDS_SETTINGS_MOUSE_SCROLL_ACCELERATION_LABEL},
      {"touchpadAccelerationLabel", IDS_SETTINGS_TOUCHPAD_ACCELERATION_LABEL},
      {"touchpadScrollAccelerationLabel",
       IDS_SETTINGS_TOUCHPAD_SCROLL_ACCELERATION_LABEL},
      {"touchpadScrollSpeed", IDS_SETTINGS_TOUCHPAD_SCROLL_SPEED_LABEL},
  };
  AddLocalizedStringsBulk(html_source, kPointersStrings);

  html_source->AddString("naturalScrollLearnMoreLink",
                         GetHelpUrlWithBoard(chrome::kNaturalScrollHelpURL));

  html_source->AddBoolean(
      "allowDisableMouseAcceleration",
      base::FeatureList::IsEnabled(::features::kAllowDisableMouseAcceleration));
  html_source->AddBoolean(
      "allowScrollSettings",
      base::FeatureList::IsEnabled(features::kAllowScrollSettings));
}

void AddDeviceKeyboardStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString keyboard_strings[] = {
      {"keyboardTitle", IDS_SETTINGS_KEYBOARD_TITLE},
      {"keyboardKeyCtrl", IDS_SETTINGS_KEYBOARD_KEY_LEFT_CTRL},
      {"keyboardKeyAlt", IDS_SETTINGS_KEYBOARD_KEY_LEFT_ALT},
      {"keyboardKeyCapsLock", IDS_SETTINGS_KEYBOARD_KEY_CAPS_LOCK},
      {"keyboardKeyCommand", IDS_SETTINGS_KEYBOARD_KEY_COMMAND},
      {"keyboardKeyDiamond", IDS_SETTINGS_KEYBOARD_KEY_DIAMOND},
      {"keyboardKeyEscape", IDS_SETTINGS_KEYBOARD_KEY_ESCAPE},
      {"keyboardKeyBackspace", IDS_SETTINGS_KEYBOARD_KEY_BACKSPACE},
      {"keyboardKeyAssistant", IDS_SETTINGS_KEYBOARD_KEY_ASSISTANT},
      {"keyboardKeyDisabled", IDS_SETTINGS_KEYBOARD_KEY_DISABLED},
      {"keyboardKeyExternalCommand",
       IDS_SETTINGS_KEYBOARD_KEY_EXTERNAL_COMMAND},
      {"keyboardKeyExternalMeta", IDS_SETTINGS_KEYBOARD_KEY_EXTERNAL_META},
      {"keyboardKeyMeta", IDS_SETTINGS_KEYBOARD_KEY_META},
      {"keyboardSendFunctionKeys", IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS},
      {"keyboardEnableAutoRepeat", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_ENABLE},
      {"keyRepeatDelay", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY},
      {"keyRepeatDelayLong", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY_LONG},
      {"keyRepeatDelayShort", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY_SHORT},
      {"keyRepeatRate", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_RATE},
      {"keyRepeatRateSlow", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_RATE_SLOW},
      {"keyRepeatRateFast", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_FAST},
      {"showKeyboardShortcutViewer",
       IDS_SETTINGS_KEYBOARD_SHOW_SHORTCUT_VIEWER},
      {"keyboardShowLanguageAndInput",
       IDS_SETTINGS_KEYBOARD_SHOW_LANGUAGE_AND_INPUT},
  };
  AddLocalizedStringsBulk(html_source, keyboard_strings);

  html_source->AddLocalizedString("keyboardKeySearch",
                                  ui::DeviceUsesKeyboardLayout2()
                                      ? IDS_SETTINGS_KEYBOARD_KEY_LAUNCHER
                                      : IDS_SETTINGS_KEYBOARD_KEY_SEARCH);
  html_source->AddLocalizedString(
      "keyboardSendFunctionKeysDescription",
      ui::DeviceUsesKeyboardLayout2()
          ? IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS_LAYOUT2_DESCRIPTION
          : IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS_DESCRIPTION);
}

void AddDeviceStylusStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kStylusStrings[] = {
      {"stylusTitle", IDS_SETTINGS_STYLUS_TITLE},
      {"stylusEnableStylusTools", IDS_SETTINGS_STYLUS_ENABLE_STYLUS_TOOLS},
      {"stylusAutoOpenStylusTools", IDS_SETTINGS_STYLUS_AUTO_OPEN_STYLUS_TOOLS},
      {"stylusFindMoreAppsPrimary", IDS_SETTINGS_STYLUS_FIND_MORE_APPS_PRIMARY},
      {"stylusFindMoreAppsSecondary",
       IDS_SETTINGS_STYLUS_FIND_MORE_APPS_SECONDARY},
      {"stylusNoteTakingApp", IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_LABEL},
      {"stylusNoteTakingAppEnabledOnLockScreen",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_LOCK_SCREEN_CHECKBOX},
      {"stylusNoteTakingAppKeepsLastNoteOnLockScreen",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_KEEP_LATEST_NOTE},
      {"stylusNoteTakingAppLockScreenSettingsHeader",
       IDS_SETTINGS_STYLUS_LOCK_SCREEN_NOTES_TITLE},
      {"stylusNoteTakingAppNoneAvailable",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_NONE_AVAILABLE},
      {"stylusNoteTakingAppWaitingForAndroid",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_WAITING_FOR_ANDROID}};
  AddLocalizedStringsBulk(html_source, kStylusStrings);
}

void AddDeviceDisplayStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kDisplayStrings[] = {
      {"displayTitle", IDS_SETTINGS_DISPLAY_TITLE},
      {"displayArrangementText", IDS_SETTINGS_DISPLAY_ARRANGEMENT_TEXT},
      {"displayArrangementTitle", IDS_SETTINGS_DISPLAY_ARRANGEMENT_TITLE},
      {"displayMirror", IDS_SETTINGS_DISPLAY_MIRROR},
      {"displayMirrorDisplayName", IDS_SETTINGS_DISPLAY_MIRROR_DISPLAY_NAME},
      {"displayAmbientColorTitle", IDS_SETTINGS_DISPLAY_AMBIENT_COLOR_TITLE},
      {"displayAmbientColorSubtitle",
       IDS_SETTINGS_DISPLAY_AMBIENT_COLOR_SUBTITLE},
      {"displayNightLightLabel", IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_LABEL},
      {"displayNightLightOnAtSunset",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_ON_AT_SUNSET},
      {"displayNightLightOffAtSunrise",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_OFF_AT_SUNRISE},
      {"displayNightLightScheduleCustom",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_CUSTOM},
      {"displayNightLightScheduleLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_LABEL},
      {"displayNightLightScheduleNever",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_NEVER},
      {"displayNightLightScheduleSunsetToSunRise",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_SUNSET_TO_SUNRISE},
      {"displayNightLightStartTime",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_START_TIME},
      {"displayNightLightStopTime", IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_STOP_TIME},
      {"displayNightLightText", IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEXT},
      {"displayNightLightTemperatureLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEMPERATURE_LABEL},
      {"displayNightLightTempSliderMaxLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEMP_SLIDER_MAX_LABEL},
      {"displayNightLightTempSliderMinLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEMP_SLIDER_MIN_LABEL},
      {"displayUnifiedDesktop", IDS_SETTINGS_DISPLAY_UNIFIED_DESKTOP},
      {"displayUnifiedDesktopOn", IDS_SETTINGS_DISPLAY_UNIFIED_DESKTOP_ON},
      {"displayUnifiedDesktopOff", IDS_SETTINGS_DISPLAY_UNIFIED_DESKTOP_OFF},
      {"displayResolutionTitle", IDS_SETTINGS_DISPLAY_RESOLUTION_TITLE},
      {"displayResolutionText", IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT},
      {"displayResolutionTextBest", IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT_BEST},
      {"displayResolutionTextNative",
       IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT_NATIVE},
      {"displayResolutionSublabel", IDS_SETTINGS_DISPLAY_RESOLUTION_SUBLABEL},
      {"displayResolutionMenuItem", IDS_SETTINGS_DISPLAY_RESOLUTION_MENU_ITEM},
      {"displayResolutionInterlacedMenuItem",
       IDS_SETTINGS_DISPLAY_RESOLUTION_INTERLACED_MENU_ITEM},
      {"displayZoomTitle", IDS_SETTINGS_DISPLAY_ZOOM_TITLE},
      {"displayZoomSublabel", IDS_SETTINGS_DISPLAY_ZOOM_SUBLABEL},
      {"displayZoomValue", IDS_SETTINGS_DISPLAY_ZOOM_VALUE},
      {"displayZoomLogicalResolutionText",
       IDS_SETTINGS_DISPLAY_ZOOM_LOGICAL_RESOLUTION_TEXT},
      {"displayZoomNativeLogicalResolutionNativeText",
       IDS_SETTINGS_DISPLAY_ZOOM_LOGICAL_RESOLUTION_NATIVE_TEXT},
      {"displayZoomLogicalResolutionDefaultText",
       IDS_SETTINGS_DISPLAY_ZOOM_LOGICAL_RESOLUTION_DEFAULT_TEXT},
      {"displaySizeSliderMinLabel", IDS_SETTINGS_DISPLAY_ZOOM_SLIDER_MINIMUM},
      {"displaySizeSliderMaxLabel", IDS_SETTINGS_DISPLAY_ZOOM_SLIDER_MAXIMUM},
      {"displayScreenTitle", IDS_SETTINGS_DISPLAY_SCREEN},
      {"displayScreenExtended", IDS_SETTINGS_DISPLAY_SCREEN_EXTENDED},
      {"displayScreenPrimary", IDS_SETTINGS_DISPLAY_SCREEN_PRIMARY},
      {"displayOrientation", IDS_SETTINGS_DISPLAY_ORIENTATION},
      {"displayOrientationStandard", IDS_SETTINGS_DISPLAY_ORIENTATION_STANDARD},
      {"displayOrientationAutoRotate",
       IDS_SETTINGS_DISPLAY_ORIENTATION_AUTO_ROTATE},
      {"displayOverscanPageText", IDS_SETTINGS_DISPLAY_OVERSCAN_TEXT},
      {"displayOverscanPageTitle", IDS_SETTINGS_DISPLAY_OVERSCAN_TITLE},
      {"displayOverscanSubtitle", IDS_SETTINGS_DISPLAY_OVERSCAN_SUBTITLE},
      {"displayOverscanInstructions",
       IDS_SETTINGS_DISPLAY_OVERSCAN_INSTRUCTIONS},
      {"displayOverscanResize", IDS_SETTINGS_DISPLAY_OVERSCAN_RESIZE},
      {"displayOverscanPosition", IDS_SETTINGS_DISPLAY_OVERSCAN_POSITION},
      {"displayOverscanReset", IDS_SETTINGS_DISPLAY_OVERSCAN_RESET},
      {"displayTouchCalibrationTitle",
       IDS_SETTINGS_DISPLAY_TOUCH_CALIBRATION_TITLE},
      {"displayTouchCalibrationText",
       IDS_SETTINGS_DISPLAY_TOUCH_CALIBRATION_TEXT}};
  AddLocalizedStringsBulk(html_source, kDisplayStrings);

  base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  html_source->AddBoolean("unifiedDesktopAvailable",
                          cmd.HasSwitch(::switches::kEnableUnifiedDesktop));

  html_source->AddBoolean("listAllDisplayModes",
                          display::features::IsListAllDisplayModesEnabled());

  html_source->AddBoolean("deviceSupportsAmbientColor",
                          ash::features::IsAllowAmbientEQEnabled());

  html_source->AddBoolean(
      "enableTouchCalibrationSetting",
      cmd.HasSwitch(chromeos::switches::kEnableTouchCalibrationSetting));

  html_source->AddBoolean("hasExternalTouchDevice",
                          display::HasExternalTouchscreenDevice());
}

void AddDeviceStorageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kStorageStrings[] = {
      {"storageTitle", IDS_SETTINGS_STORAGE_TITLE},
      {"storageItemInUse", IDS_SETTINGS_STORAGE_ITEM_IN_USE},
      {"storageItemAvailable", IDS_SETTINGS_STORAGE_ITEM_AVAILABLE},
      {"storageItemSystem", IDS_SETTINGS_STORAGE_ITEM_SYSTEM},
      {"storageItemMyFiles", IDS_SETTINGS_STORAGE_ITEM_MY_FILES},
      {"storageItemBrowsingData", IDS_SETTINGS_STORAGE_ITEM_BROWSING_DATA},
      {"storageItemApps", IDS_SETTINGS_STORAGE_ITEM_APPS},
      {"storageItemCrostini", IDS_SETTINGS_STORAGE_ITEM_CROSTINI},
      {"storageItemOtherUsers", IDS_SETTINGS_STORAGE_ITEM_OTHER_USERS},
      {"storageSizeComputing", IDS_SETTINGS_STORAGE_SIZE_CALCULATING},
      {"storageSizeUnknown", IDS_SETTINGS_STORAGE_SIZE_UNKNOWN},
      {"storageSpaceLowMessageTitle",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_TITLE},
      {"storageSpaceLowMessageLine1",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_LINE_1},
      {"storageSpaceLowMessageLine2",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_LINE_2},
      {"storageSpaceCriticallyLowMessageTitle",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_TITLE},
      {"storageSpaceCriticallyLowMessageLine1",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_LINE_1},
      {"storageSpaceCriticallyLowMessageLine2",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_LINE_2},
      {"storageExternal", IDS_SETTINGS_STORAGE_EXTERNAL},
      {"storageExternalStorageEmptyListHeader",
       IDS_SETTINGS_STORAGE_EXTERNAL_STORAGE_EMPTY_LIST_HEADER},
      {"storageExternalStorageListHeader",
       IDS_SETTINGS_STORAGE_EXTERNAL_STORAGE_LIST_HEADER},
      {"storageOverviewAriaLabel", IDS_SETTINGS_STORAGE_OVERVIEW_ARIA_LABEL}};
  AddLocalizedStringsBulk(html_source, kStorageStrings);

  html_source->AddString(
      "storageAndroidAppsExternalDrivesNote",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_STORAGE_ANDROID_APPS_ACCESS_EXTERNAL_DRIVES_NOTE,
          base::ASCIIToUTF16(chrome::kArcExternalStorageLearnMoreURL)));
}

void AddDevicePowerStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kPowerStrings[] = {
      {"powerTitle", IDS_SETTINGS_POWER_TITLE},
      {"powerSourceLabel", IDS_SETTINGS_POWER_SOURCE_LABEL},
      {"powerSourceBattery", IDS_SETTINGS_POWER_SOURCE_BATTERY},
      {"powerSourceAcAdapter", IDS_SETTINGS_POWER_SOURCE_AC_ADAPTER},
      {"powerSourceLowPowerCharger",
       IDS_SETTINGS_POWER_SOURCE_LOW_POWER_CHARGER},
      {"calculatingPower", IDS_SETTINGS_POWER_SOURCE_CALCULATING},
      {"powerIdleLabel", IDS_SETTINGS_POWER_IDLE_LABEL},
      {"powerIdleDisplayOffSleep", IDS_SETTINGS_POWER_IDLE_DISPLAY_OFF_SLEEP},
      {"powerIdleDisplayOff", IDS_SETTINGS_POWER_IDLE_DISPLAY_OFF},
      {"powerIdleDisplayOn", IDS_SETTINGS_POWER_IDLE_DISPLAY_ON},
      {"powerIdleOther", IDS_SETTINGS_POWER_IDLE_OTHER},
      {"powerLidSleepLabel", IDS_SETTINGS_POWER_LID_CLOSED_SLEEP_LABEL},
      {"powerLidSignOutLabel", IDS_SETTINGS_POWER_LID_CLOSED_SIGN_OUT_LABEL},
      {"powerLidShutDownLabel", IDS_SETTINGS_POWER_LID_CLOSED_SHUT_DOWN_LABEL},
  };
  AddLocalizedStringsBulk(html_source, kPowerStrings);
}

void AddDeviceStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kDeviceStrings[] = {
      {"devicePageTitle", IDS_SETTINGS_DEVICE_TITLE},
      {"scrollLabel", IDS_SETTINGS_SCROLL_LABEL},
      {"touchPadScrollLabel", IDS_OS_SETTINGS_TOUCHPAD_REVERSE_SCROLL_LABEL},
  };
  AddLocalizedStringsBulk(html_source, kDeviceStrings);

  AddDevicePointersStrings(html_source);
  AddDeviceKeyboardStrings(html_source);
  AddDeviceStylusStrings(html_source);
  AddDeviceDisplayStrings(html_source);
  AddDeviceStorageStrings(html_source);
  AddDevicePowerStrings(html_source);
}

void AddFilesStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"disconnectGoogleDriveAccount", IDS_SETTINGS_DISCONNECT_GOOGLE_DRIVE},
      {"filesPageTitle", IDS_OS_SETTINGS_FILES},
      {"smbSharesTitle", IDS_SETTINGS_DOWNLOADS_SMB_SHARES},
      {"smbSharesLearnMoreLabel",
       IDS_SETTINGS_DOWNLOADS_SMB_SHARES_LEARN_MORE_LABEL},
      {"addSmbShare", IDS_SETTINGS_DOWNLOADS_SMB_SHARES_ADD_SHARE},
      {"smbShareAddedSuccessfulMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_SUCCESS_MESSAGE},
      {"smbShareAddedErrorMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_ERROR_MESSAGE},
      {"smbShareAddedAuthFailedMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_AUTH_FAILED_MESSAGE},
      {"smbShareAddedNotFoundMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_NOT_FOUND_MESSAGE},
      {"smbShareAddedUnsupportedDeviceMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_UNSUPPORTED_DEVICE_MESSAGE},
      {"smbShareAddedMountExistsMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_EXISTS_MESSAGE},
      {"smbShareAddedTooManyMountsMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_TOO_MANY_MOUNTS_MESSAGE},
      {"smbShareAddedInvalidURLMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_INVALID_URL_MESSAGE},
      {"smbShareAddedInvalidSSOURLMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_INVALID_SSO_URL_MESSAGE},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  chromeos::smb_dialog::AddLocalizedStrings(html_source);

  html_source->AddString("smbSharesLearnMoreURL",
                         GetHelpUrlWithBoard(chrome::kSmbSharesLearnMoreURL));
}

void AddEasyUnlockStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"easyUnlockSectionTitle", IDS_SETTINGS_EASY_UNLOCK_SECTION_TITLE},
      {"easyUnlockUnlockDeviceOnly",
       IDS_SETTINGS_EASY_UNLOCK_UNLOCK_DEVICE_ONLY},
      {"easyUnlockUnlockDeviceAndAllowSignin",
       IDS_SETTINGS_EASY_UNLOCK_UNLOCK_DEVICE_AND_ALLOW_SIGNIN},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

void AddMultideviceStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"multidevicePageTitle", IDS_SETTINGS_MULTIDEVICE},
      {"multideviceSetupButton", IDS_SETTINGS_MULTIDEVICE_SETUP_BUTTON},
      {"multideviceVerifyButton", IDS_SETTINGS_MULTIDEVICE_VERIFY_BUTTON},
      {"multideviceSetupItemHeading",
       IDS_SETTINGS_MULTIDEVICE_SETUP_ITEM_HEADING},
      {"multideviceEnabled", IDS_SETTINGS_MULTIDEVICE_ENABLED},
      {"multideviceDisabled", IDS_SETTINGS_MULTIDEVICE_DISABLED},
      {"multideviceSmartLockItemTitle", IDS_SETTINGS_EASY_UNLOCK_SECTION_TITLE},
      {"multideviceInstantTetheringItemTitle",
       IDS_SETTINGS_MULTIDEVICE_INSTANT_TETHERING},
      {"multideviceInstantTetheringItemSummary",
       IDS_SETTINGS_MULTIDEVICE_INSTANT_TETHERING_SUMMARY},
      {"multideviceAndroidMessagesItemTitle",
       IDS_SETTINGS_MULTIDEVICE_ANDROID_MESSAGES},
      {"multideviceForgetDevice", IDS_SETTINGS_MULTIDEVICE_FORGET_THIS_DEVICE},
      {"multideviceSmartLockOptions",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_OPTIONS_LOCK},
      {"multideviceForgetDeviceDisconnect",
       IDS_SETTINGS_MULTIDEVICE_FORGET_THIS_DEVICE_DISCONNECT},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString(
      "multideviceForgetDeviceSummary",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_FORGET_THIS_DEVICE_EXPLANATION));
  html_source->AddString(
      "multideviceForgetDeviceDialogMessage",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_FORGET_DEVICE_DIALOG_MESSAGE));
  html_source->AddString(
      "multideviceVerificationText",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_VERIFICATION_TEXT,
          base::UTF8ToUTF16(
              chromeos::multidevice_setup::
                  GetBoardSpecificBetterTogetherSuiteLearnMoreUrl()
                      .spec())));
  html_source->AddString(
      "multideviceSetupSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_SETUP_SUMMARY, ui::GetChromeOSDeviceName(),
          base::UTF8ToUTF16(
              chromeos::multidevice_setup::
                  GetBoardSpecificBetterTogetherSuiteLearnMoreUrl()
                      .spec())));
  html_source->AddString(
      "multideviceNoHostText",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_NO_ELIGIBLE_HOSTS,
          base::UTF8ToUTF16(
              chromeos::multidevice_setup::
                  GetBoardSpecificBetterTogetherSuiteLearnMoreUrl()
                      .spec())));
  html_source->AddString(
      "multideviceAndroidMessagesItemSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_ANDROID_MESSAGES_SUMMARY,
          ui::GetChromeOSDeviceName(),
          base::UTF8ToUTF16(chromeos::multidevice_setup::
                                GetBoardSpecificMessagesLearnMoreUrl()
                                    .spec())));
  html_source->AddString(
      "multideviceSmartLockItemSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_SMART_LOCK_SUMMARY,
          ui::GetChromeOSDeviceName(),
          GetHelpUrlWithBoard(chrome::kEasyUnlockLearnMoreUrl)));

  AddEasyUnlockStrings(html_source);
}

void AddKerberosAccountsPageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"kerberosAccountsAddAccountLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_ADD_ACCOUNT_LABEL},
      {"kerberosAccountsRefreshNowLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_REFRESH_NOW_LABEL},
      {"kerberosAccountsSetAsActiveAccountLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_SET_AS_ACTIVE_ACCOUNT_LABEL},
      {"kerberosAccountsSignedOut", IDS_SETTINGS_KERBEROS_ACCOUNTS_SIGNED_OUT},
      {"kerberosAccountsListHeader",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_LIST_HEADER},
      {"kerberosAccountsRemoveAccountLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_REMOVE_ACCOUNT_LABEL},
      {"kerberosAccountsReauthenticationLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_REAUTHENTICATION_LABEL},
      {"kerberosAccountsTicketActive",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_TICKET_ACTIVE},
      {"kerberosAccountsAccountRemovedTip",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_ACCOUNT_REMOVED_TIP},
      {"kerberosAccountsAccountRefreshedTip",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_ACCOUNT_REFRESHED_TIP},
      {"kerberosAccountsSignedIn", IDS_SETTINGS_KERBEROS_ACCOUNTS_SIGNED_IN},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  PrefService* local_state = g_browser_process->local_state();

  // Whether new Kerberos accounts may be added.
  html_source->AddBoolean(
      "kerberosAddAccountsAllowed",
      local_state->GetBoolean(prefs::kKerberosAddAccountsAllowed));

  // Kerberos accounts page with "Learn more" link.
  html_source->AddString(
      "kerberosAccountsDescription",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_KERBEROS_ACCOUNTS_DESCRIPTION,
          GetHelpUrlWithBoard(chrome::kKerberosAccountsLearnMoreURL)));
}

void AddKerberosAddAccountDialogStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"kerberosAccountsAdvancedConfigLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_ADVANCED_CONFIG_LABEL},
      {"kerberosAdvancedConfigTitle",
       IDS_SETTINGS_KERBEROS_ADVANCED_CONFIG_TITLE},
      {"kerberosAdvancedConfigDesc",
       IDS_SETTINGS_KERBEROS_ADVANCED_CONFIG_DESC},
      {"addKerberosAccountRememberPassword",
       IDS_SETTINGS_ADD_KERBEROS_ACCOUNT_REMEMBER_PASSWORD},
      {"kerberosPassword", IDS_SETTINGS_KERBEROS_PASSWORD},
      {"kerberosUsername", IDS_SETTINGS_KERBEROS_USERNAME},
      {"addKerberosAccountDescription",
       IDS_SETTINGS_ADD_KERBEROS_ACCOUNT_DESCRIPTION},
      {"kerberosErrorNetworkProblem",
       IDS_SETTINGS_KERBEROS_ERROR_NETWORK_PROBLEM},
      {"kerberosErrorUsernameInvalid",
       IDS_SETTINGS_KERBEROS_ERROR_USERNAME_INVALID},
      {"kerberosErrorUsernameUnknown",
       IDS_SETTINGS_KERBEROS_ERROR_USERNAME_UNKNOWN},
      {"kerberosErrorDuplicatePrincipalName",
       IDS_SETTINGS_KERBEROS_ERROR_DUPLICATE_PRINCIPAL_NAME},
      {"kerberosErrorContactingServer",
       IDS_SETTINGS_KERBEROS_ERROR_CONTACTING_SERVER},
      {"kerberosErrorPasswordInvalid",
       IDS_SETTINGS_KERBEROS_ERROR_PASSWORD_INVALID},
      {"kerberosErrorPasswordExpired",
       IDS_SETTINGS_KERBEROS_ERROR_PASSWORD_EXPIRED},
      {"kerberosErrorKdcEncType", IDS_SETTINGS_KERBEROS_ERROR_KDC_ENC_TYPE},
      {"kerberosErrorGeneral", IDS_SETTINGS_KERBEROS_ERROR_GENERAL},
      {"kerberosConfigErrorSectionNestedInGroup",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_SECTION_NESTED_IN_GROUP},
      {"kerberosConfigErrorSectionSyntax",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_SECTION_SYNTAX},
      {"kerberosConfigErrorExpectedOpeningCurlyBrace",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_EXPECTED_OPENING_CURLY_BRACE},
      {"kerberosConfigErrorExtraCurlyBrace",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_EXTRA_CURLY_BRACE},
      {"kerberosConfigErrorRelationSyntax",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_RELATION_SYNTAX_ERROR},
      {"kerberosConfigErrorKeyNotSupported",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_KEY_NOT_SUPPORTED},
      {"kerberosConfigErrorSectionNotSupported",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_SECTION_NOT_SUPPORTED},
      {"kerberosConfigErrorKrb5FailedToParse",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_KRB5_FAILED_TO_PARSE},
      {"addKerberosAccountRefreshButtonLabel",
       IDS_SETTINGS_ADD_KERBEROS_ACCOUNT_REFRESH_BUTTON_LABEL},
      {"addKerberosAccount", IDS_SETTINGS_ADD_KERBEROS_ACCOUNT},
      {"refreshKerberosAccount", IDS_SETTINGS_REFRESH_KERBEROS_ACCOUNT},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  PrefService* local_state = g_browser_process->local_state();

  // Whether the 'Remember password' checkbox is enabled.
  html_source->AddBoolean(
      "kerberosRememberPasswordEnabled",
      local_state->GetBoolean(prefs::kKerberosRememberPasswordEnabled));

  // Kerberos default configuration.
  html_source->AddString(
      "defaultKerberosConfig",
      chromeos::KerberosCredentialsManager::GetDefaultKerberosConfig());
}

void AddLockScreenPageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"lockScreenNotificationTitle",
       IDS_ASH_SETTINGS_LOCK_SCREEN_NOTIFICATION_TITLE},
      {"lockScreenNotificationHideSensitive",
       IDS_ASH_SETTINGS_LOCK_SCREEN_NOTIFICATION_HIDE_SENSITIVE},
      {"enableScreenlock", IDS_SETTINGS_PEOPLE_ENABLE_SCREENLOCK},
      {"lockScreenNotificationShow",
       IDS_ASH_SETTINGS_LOCK_SCREEN_NOTIFICATION_SHOW},
      {"lockScreenPinOrPassword",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PIN_OR_PASSWORD},
      {"lockScreenSetupFingerprintButton",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_SETUP_BUTTON},
      {"lockScreenNotificationHide",
       IDS_ASH_SETTINGS_LOCK_SCREEN_NOTIFICATION_HIDE},
      {"lockScreenEditFingerprints",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_EDIT_FINGERPRINTS},
      {"lockScreenPasswordOnly", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PASSWORD_ONLY},
      {"lockScreenChangePinButton",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_CHANGE_PIN_BUTTON},
      {"lockScreenEditFingerprintsDescription",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_EDIT_FINGERPRINTS_DESCRIPTION},
      {"lockScreenNumberFingerprints",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NUM_FINGERPRINTS},
      {"lockScreenNone", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NONE},
      {"lockScreenFingerprintNewName",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NEW_FINGERPRINT_DEFAULT_NAME},
      {"lockScreenDeleteFingerprintLabel",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_DELETE_FINGERPRINT_ARIA_LABEL},
      {"lockScreenOptionsLock", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_OPTIONS_LOCK},
      {"lockScreenOptionsLoginLock",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_OPTIONS_LOGIN_LOCK},
      {"lockScreenSetupPinButton",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_SETUP_PIN_BUTTON},
      {"lockScreenTitleLock", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE_LOCK},
      {"lockScreenTitleLoginLock",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE_LOGIN_LOCK},
      {"passwordPromptEnterPasswordLock",
       IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_ENTER_PASSWORD_LOCK},
      {"passwordPromptEnterPasswordLoginLock",
       IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_ENTER_PASSWORD_LOGIN_LOCK},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

void AddUsersStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"usersModifiedByOwnerLabel", IDS_SETTINGS_USERS_MODIFIED_BY_OWNER_LABEL},
      {"guestBrowsingLabel", IDS_SETTINGS_USERS_GUEST_BROWSING_LABEL},
      {"settingsManagedLabel", IDS_SETTINGS_USERS_MANAGED_LABEL},
      {"showOnSigninLabel", IDS_SETTINGS_USERS_SHOW_ON_SIGNIN_LABEL},
      {"restrictSigninLabel", IDS_SETTINGS_USERS_RESTRICT_SIGNIN_LABEL},
      {"deviceOwnerLabel", IDS_SETTINGS_USERS_DEVICE_OWNER_LABEL},
      {"removeUserTooltip", IDS_SETTINGS_USERS_REMOVE_USER_TOOLTIP},
      {"addUsers", IDS_SETTINGS_USERS_ADD_USERS},
      {"addUsersEmail", IDS_SETTINGS_USERS_ADD_USERS_EMAIL},
      {"userExistsError", IDS_SETTINGS_USER_EXISTS_ERROR},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

void AddGoogleAssistantStrings(content::WebUIDataSource* html_source,
                               Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"googleAssistantPageTitle", IDS_SETTINGS_GOOGLE_ASSISTANT},
      {"googleAssistantEnableContext", IDS_ASSISTANT_SCREEN_CONTEXT_TITLE},
      {"googleAssistantEnableContextDescription",
       IDS_ASSISTANT_SCREEN_CONTEXT_DESC},
      {"googleAssistantEnableHotword",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD},
      {"googleAssistantEnableHotwordDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_DESCRIPTION},
      {"googleAssistantVoiceSettings",
       IDS_SETTINGS_GOOGLE_ASSISTANT_VOICE_SETTINGS},
      {"googleAssistantVoiceSettingsDescription",
       IDS_ASSISTANT_VOICE_MATCH_RECORDING},
      {"googleAssistantVoiceSettingsRetrainButton",
       IDS_SETTINGS_GOOGLE_ASSISTANT_VOICE_SETTINGS_RETRAIN},
      {"googleAssistantEnableHotwordWithoutDspDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_WITHOUT_DSP_DESCRIPTION},
      {"googleAssistantEnableHotwordWithoutDspRecommended",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_WITHOUT_DSP_RECOMMENDED},
      {"googleAssistantEnableHotwordWithoutDspAlwaysOn",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_WITHOUT_DSP_ALWAYS_ON},
      {"googleAssistantEnableHotwordWithoutDspOff",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_WITHOUT_DSP_OFF},
      {"googleAssistantEnableNotification",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_NOTIFICATION},
      {"googleAssistantEnableNotificationDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_NOTIFICATION_DESCRIPTION},
      {"googleAssistantLaunchWithMicOpen",
       IDS_SETTINGS_GOOGLE_ASSISTANT_LAUNCH_WITH_MIC_OPEN},
      {"googleAssistantLaunchWithMicOpenDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_LAUNCH_WITH_MIC_OPEN_DESCRIPTION},
      {"googleAssistantSettings", IDS_SETTINGS_GOOGLE_ASSISTANT_SETTINGS},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean("hotwordDspAvailable",
                          chromeos::IsHotwordDspAvailable());
  html_source->AddBoolean(
      "voiceMatchDisabled",
      chromeos::assistant::features::IsVoiceMatchDisabled());
}

void AddPrintingStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"printingPageTitle", IDS_SETTINGS_PRINTING},
      {"cupsPrintersTitle", IDS_SETTINGS_PRINTING_CUPS_PRINTERS},
      {"cupsPrintersLearnMoreLabel",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_LEARN_MORE_LABEL},
      {"addCupsPrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_ADD_PRINTER},
      {"editPrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_EDIT},
      {"removePrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_REMOVE},
      {"setupPrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTER_SETUP_BUTTON},
      {"setupPrinterAria",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_SETUP_BUTTON_ARIA},
      {"savePrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTER_SAVE_BUTTON},
      {"savePrinterAria", IDS_SETTINGS_PRINTING_CUPS_PRINTER_SAVE_BUTTON_ARIA},
      {"searchLabel", IDS_SETTINGS_PRINTING_CUPS_SEARCH_LABEL},
      {"noSearchResults", IDS_SEARCH_NO_RESULTS},
      {"printerDetailsTitle", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_TITLE},
      {"printerName", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_NAME},
      {"printerModel", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_MODEL},
      {"printerQueue", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_QUEUE},
      {"savedPrintersTitle", IDS_SETTINGS_PRINTING_CUPS_SAVED_PRINTERS_TITLE},
      {"savedPrintersCountMany",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_SAVED_PRINTERS_COUNT_MANY},
      {"savedPrintersCountOne",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_SAVED_PRINTERS_COUNT_ONE},
      {"savedPrintersCountNone",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_SAVED_PRINTERS_COUNT_NONE},
      {"showMorePrinters", IDS_SETTINGS_PRINTING_CUPS_SHOW_MORE},
      {"addPrintersNearbyTitle",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTERS_NEARBY_TITLE},
      {"addPrintersManuallyTitle",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTERS_MANUALLY_TITLE},
      {"manufacturerAndModelDialogTitle",
       IDS_SETTINGS_PRINTING_CUPS_SELECT_MANUFACTURER_AND_MODEL_TITLE},
      {"nearbyPrintersListTitle",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTERS},
      {"nearbyPrintersCountMany",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTERS_COUNT_MANY},
      {"nearbyPrintersCountOne",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTER_COUNT_ONE},
      {"nearbyPrintersCountNone",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTER_COUNT_NONE},
      {"nearbyPrintersListDescription",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_ADD_DETECTED_OR_NEW_PRINTER},
      {"manufacturerAndModelAdditionalInformation",
       IDS_SETTINGS_PRINTING_CUPS_MANUFACTURER_MODEL_ADDITIONAL_INFORMATION},
      {"addPrinterButtonText",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_ADD},
      {"printerDetailsAdvanced", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED},
      {"printerDetailsA11yLabel",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_ACCESSIBILITY_LABEL},
      {"printerAddress", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_ADDRESS},
      {"printerProtocol", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_PROTOCOL},
      {"printerURI", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_URI},
      {"manuallyAddPrinterButtonText",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_MANUAL_ADD},
      {"discoverPrintersButtonText",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_DISCOVER_PRINTERS},
      {"printerProtocolIpp", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_IPP},
      {"printerProtocolIpps", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_IPPS},
      {"printerProtocolHttp", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_HTTP},
      {"printerProtocolHttps",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_HTTPS},
      {"printerProtocolAppSocket",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_APP_SOCKET},
      {"printerProtocolLpd", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_LPD},
      {"printerProtocolUsb", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_USB},
      {"printerProtocolIppUsb",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_IPPUSB},
      {"printerConfiguringMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_CONFIGURING_MESSAGE},
      {"printerManufacturer", IDS_SETTINGS_PRINTING_CUPS_PRINTER_MANUFACTURER},
      {"selectDriver", IDS_SETTINGS_PRINTING_CUPS_PRINTER_SELECT_DRIVER},
      {"selectDriverButtonText",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_BUTTON_SELECT_DRIVER},
      {"selectDriverButtonAriaLabel",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_BUTTON_SELECT_DRIVER_ARIA_LABEL},
      {"selectDriverErrorMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_INVALID_DRIVER},
      {"printerAddedSuccessfulMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_DONE_MESSAGE},
      {"printerEditedSuccessfulMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_EDITED_PRINTER_DONE_MESSAGE},
      {"printerUnavailableMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_UNAVAILABLE_MESSAGE},
      {"noPrinterNearbyMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_NO_PRINTER_NEARBY},
      {"searchingNearbyPrinters",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_SEARCHING_NEARBY_PRINTER},
      {"printerAddedFailedMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_ERROR_MESSAGE},
      {"printerAddedFatalErrorMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_FATAL_ERROR_MESSAGE},
      {"printerAddedUnreachableMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PRINTER_UNREACHABLE_MESSAGE},
      {"printerAddedPpdTooLargeMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PPD_TOO_LARGE_MESSAGE},
      {"printerAddedInvalidPpdMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_INVALID_PPD_MESSAGE},
      {"printerAddedPpdNotFoundMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PPD_NOT_FOUND},
      {"printerAddedPpdUnretrievableMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PPD_UNRETRIEVABLE},
      {"printerAddedNativePrintersNotAllowedMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_NATIVE_PRINTERS_NOT_ALLOWED_MESSAGE},
      {"editPrinterInvalidPrinterUpdate",
       IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_INVALID_PRINTER_UPDATE},
      {"requireNetworkMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_REQUIRE_INTERNET_MESSAGE},
      {"checkNetworkMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_CHECK_CONNECTION_MESSAGE},
      {"noInternetConnection",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_NO_INTERNET_CONNECTION},
      {"checkNetworkAndTryAgain",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_CONNECT_TO_NETWORK_SUBTEXT},
      {"editPrinterDialogTitle",
       IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_DIALOG_TITLE},
      {"editPrinterButtonText", IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_BUTTON},
      {"currentPpdMessage",
       IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_CURRENT_PPD_MESSAGE},
      {"printerEulaNotice", IDS_SETTINGS_PRINTING_CUPS_EULA_NOTICE},
      {"ippPrinterUnreachable", IDS_SETTINGS_PRINTING_CUPS_IPP_URI_UNREACHABLE},
      {"generalPrinterDialogError",
       IDS_SETTINGS_PRINTING_CUPS_DIALOG_GENERAL_ERROR},
      {"printServerButtonText", IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER},
      {"addPrintServerTitle",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINT_SERVER_TITLE},
      {"printServerAddress", IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_ADDRESS},
      {"printServerFoundZeroPrinters",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_FOUND_ZERO_PRINTERS},
      {"printServerFoundOnePrinter",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_FOUND_ONE_PRINTER},
      {"printServerFoundManyPrinters",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_FOUND_MANY_PRINTERS},
      {"printServerInvalidUrlAddress",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_INVALID_URL_ADDRESS},
      {"printServerConnectionError",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_CONNECTION_ERROR},
      {"printServerConfigurationErrorMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_REACHABLE_BUT_CANNOT_ADD},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString("printingCUPSPrintLearnMoreUrl",
                         GetHelpUrlWithBoard(chrome::kCupsPrintLearnMoreURL));
  html_source->AddString(
      "printingCUPSPrintPpdLearnMoreUrl",
      GetHelpUrlWithBoard(chrome::kCupsPrintPPDLearnMoreURL));
  html_source->AddBoolean(
      "consumerPrintServerUiEnabled",
      base::FeatureList::IsEnabled(::features::kPrintServerUi));
}

void AddSearchInSettingsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"searchPrompt", IDS_SETTINGS_SEARCH_PROMPT},
      {"searchNoResults", IDS_SEARCH_NO_RESULTS},
      {"searchResults", IDS_SEARCH_RESULTS},
      // TODO(dpapad): IDS_DOWNLOAD_CLEAR_SEARCH and IDS_HISTORY_CLEAR_SEARCH
      // are identical, merge them to one and re-use here.
      {"clearSearch", IDS_DOWNLOAD_CLEAR_SEARCH},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString(
      "searchNoOsResultsHelp",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SEARCH_NO_RESULTS_HELP,
          base::ASCIIToUTF16(chrome::kOsSettingsSearchHelpURL)));

  html_source->AddBoolean(
      "newOsSettingsSearch",
      base::FeatureList::IsEnabled(chromeos::features::kNewOsSettingsSearch));
}

void AddDateTimeStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"dateTimePageTitle", IDS_SETTINGS_DATE_TIME},
      {"timeZone", IDS_SETTINGS_TIME_ZONE},
      {"selectTimeZoneResolveMethod",
       IDS_SETTINGS_SELECT_TIME_ZONE_RESOLVE_METHOD},
      {"timeZoneGeolocation", IDS_SETTINGS_TIME_ZONE_GEOLOCATION},
      {"timeZoneButton", IDS_SETTINGS_TIME_ZONE_BUTTON},
      {"timeZoneSubpageTitle", IDS_SETTINGS_TIME_ZONE_SUBPAGE_TITLE},
      {"setTimeZoneAutomaticallyDisabled",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_DISABLED},
      {"setTimeZoneAutomaticallyOn",
       IDS_SETTINGS_TIME_ZONE_DETECTION_SET_AUTOMATICALLY},
      {"setTimeZoneAutomaticallyOff",
       IDS_SETTINGS_TIME_ZONE_DETECTION_CHOOSE_FROM_LIST},
      {"setTimeZoneAutomaticallyIpOnlyDefault",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_IP_ONLY_DEFAULT},
      {"setTimeZoneAutomaticallyWithWiFiAccessPointsData",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_SEND_WIFI_AP},
      {"setTimeZoneAutomaticallyWithAllLocationInfo",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_SEND_ALL_INFO},
      {"use24HourClock", IDS_SETTINGS_USE_24_HOUR_CLOCK},
      {"setDateTime", IDS_SETTINGS_SET_DATE_TIME},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString(
      "timeZoneSettingsLearnMoreURL",
      base::ASCIIToUTF16(base::StringPrintf(
          chrome::kTimeZoneSettingsLearnMoreURL,
          g_browser_process->GetApplicationLocale().c_str())));
}

void AddAboutStrings(content::WebUIDataSource* html_source, Profile* profile) {
  // Top level About page strings.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"aboutProductLogoAlt", IDS_SHORT_PRODUCT_LOGO_ALT_TEXT},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"aboutReportAnIssue", IDS_SETTINGS_ABOUT_PAGE_REPORT_AN_ISSUE},
#endif
    {"aboutRelaunch", IDS_SETTINGS_ABOUT_PAGE_RELAUNCH},
    {"aboutUpgradeCheckStarted", IDS_SETTINGS_ABOUT_UPGRADE_CHECK_STARTED},
    {"aboutUpgradeRelaunch", IDS_SETTINGS_UPGRADE_SUCCESSFUL_RELAUNCH},
    {"aboutUpgradeUpdating", IDS_SETTINGS_UPGRADE_UPDATING},
    {"aboutUpgradeUpdatingPercent", IDS_SETTINGS_UPGRADE_UPDATING_PERCENT},
    {"aboutGetHelpUsingChrome", IDS_SETTINGS_GET_HELP_USING_CHROME},
    {"aboutPageTitle", IDS_SETTINGS_ABOUT_PROGRAM},
    {"aboutProductTitle", IDS_PRODUCT_NAME},

    {"aboutEndOfLifeTitle", IDS_SETTINGS_ABOUT_PAGE_END_OF_LIFE_TITLE},
    {"aboutRelaunchAndPowerwash",
     IDS_SETTINGS_ABOUT_PAGE_RELAUNCH_AND_POWERWASH},
    {"aboutRollbackInProgress", IDS_SETTINGS_UPGRADE_ROLLBACK_IN_PROGRESS},
    {"aboutRollbackSuccess", IDS_SETTINGS_UPGRADE_ROLLBACK_SUCCESS},
    {"aboutUpgradeUpdatingChannelSwitch",
     IDS_SETTINGS_UPGRADE_UPDATING_CHANNEL_SWITCH},
    {"aboutUpgradeSuccessChannelSwitch",
     IDS_SETTINGS_UPGRADE_SUCCESSFUL_CHANNEL_SWITCH},
    {"aboutTPMFirmwareUpdateTitle",
     IDS_SETTINGS_ABOUT_TPM_FIRMWARE_UPDATE_TITLE},
    {"aboutTPMFirmwareUpdateDescription",
     IDS_SETTINGS_ABOUT_TPM_FIRMWARE_UPDATE_DESCRIPTION},

    // About page, channel switcher dialog.
    {"aboutChangeChannel", IDS_SETTINGS_ABOUT_PAGE_CHANGE_CHANNEL},
    {"aboutChangeChannelAndPowerwash",
     IDS_SETTINGS_ABOUT_PAGE_CHANGE_CHANNEL_AND_POWERWASH},
    {"aboutDelayedWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_DELAYED_WARNING_MESSAGE},
    {"aboutDelayedWarningTitle", IDS_SETTINGS_ABOUT_PAGE_DELAYED_WARNING_TITLE},
    {"aboutPowerwashWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_POWERWASH_WARNING_MESSAGE},
    {"aboutPowerwashWarningTitle",
     IDS_SETTINGS_ABOUT_PAGE_POWERWASH_WARNING_TITLE},
    {"aboutUnstableWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_UNSTABLE_WARNING_MESSAGE},
    {"aboutUnstableWarningTitle",
     IDS_SETTINGS_ABOUT_PAGE_UNSTABLE_WARNING_TITLE},
    {"aboutChannelDialogBeta", IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_BETA},
    {"aboutChannelDialogDev", IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_DEV},
    {"aboutChannelDialogStable", IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_STABLE},

    // About page, update warning dialog.
    {"aboutUpdateWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_UPDATE_WARNING_MESSAGE},
    {"aboutUpdateWarningTitle", IDS_SETTINGS_ABOUT_PAGE_UPDATE_WARNING_TITLE},

    // Detailed build information
    {"aboutBuildDetailsTitle", IDS_OS_SETTINGS_ABOUT_PAGE_BUILD_DETAILS},
    {"aboutChannelBeta", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_BETA},
    {"aboutChannelCanary", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_CANARY},
    {"aboutChannelDev", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_DEV},
    {"aboutChannelLabel", IDS_SETTINGS_ABOUT_PAGE_CHANNEL},
    {"aboutChannelStable", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_STABLE},
    {"aboutCheckForUpdates", IDS_SETTINGS_ABOUT_PAGE_CHECK_FOR_UPDATES},
    {"aboutCurrentlyOnChannel", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL},
    {"aboutDetailedBuildInfo", IDS_SETTINGS_ABOUT_PAGE_DETAILED_BUILD_INFO},
    {version_ui::kApplicationLabel, IDS_PRODUCT_NAME},
    {version_ui::kPlatform, IDS_PLATFORM_LABEL},
    {version_ui::kFirmwareVersion, IDS_VERSION_UI_FIRMWARE_VERSION},
    {version_ui::kARC, IDS_ARC_LABEL},
    {"aboutBuildDetailsCopyTooltipLabel",
     IDS_OS_SETTINGS_ABOUT_PAGE_BUILD_DETAILS_COPY_TOOLTIP_LABEL},
    {"aboutIsArcStatusTitle", IDS_OS_SETTINGS_ABOUT_ARC_STATUS_TITLE},
    {"aboutIsDeveloperModeTitle", IDS_OS_SETTINGS_ABOUT_DEVELOPER_MODE},
    {"isEnterpriseManagedTitle",
     IDS_OS_SETTINGS_ABOUT_PAGE_ENTERPRISE_ENNROLLED_TITLE},
    {"aboutOsPageTitle", IDS_SETTINGS_ABOUT_OS},
    {"aboutGetHelpUsingChromeOs", IDS_SETTINGS_GET_HELP_USING_CHROME_OS},
    {"aboutOsProductTitle", IDS_PRODUCT_OS_NAME},
    {"aboutReleaseNotesOffline", IDS_SETTINGS_ABOUT_PAGE_RELEASE_NOTES},
    {"aboutShowReleaseNotes", IDS_SETTINGS_ABOUT_PAGE_SHOW_RELEASE_NOTES},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString("aboutTPMFirmwareUpdateLearnMoreURL",
                         chrome::kTPMFirmwareUpdateLearnMoreURL);
  html_source->AddString(
      "aboutUpgradeUpToDate",
      ui::SubstituteChromeOSDeviceType(IDS_SETTINGS_UPGRADE_UP_TO_DATE));
  html_source->AddString("managementPage",
                         ManagementUI::GetManagementPageSubtitle(profile));
}

void AddResetStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"resetPageTitle", IDS_SETTINGS_RESET},
      {"powerwashTitle", IDS_SETTINGS_FACTORY_RESET},
      {"powerwashDialogTitle", IDS_SETTINGS_FACTORY_RESET_HEADING},
      {"powerwashDialogButton", IDS_SETTINGS_RESTART},
      {"powerwashButton", IDS_SETTINGS_FACTORY_RESET_BUTTON_LABEL},
      {"powerwashDialogExplanation", IDS_SETTINGS_FACTORY_RESET_WARNING},
      {"powerwashLearnMoreUrl", IDS_FACTORY_RESET_HELP_URL},
      {"powerwashButtonRoleDescription",
       IDS_SETTINGS_FACTORY_RESET_BUTTON_ROLE},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString(
      "powerwashDescription",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_FACTORY_RESET_DESCRIPTION,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
}

void AddSearchStrings(content::WebUIDataSource* html_source, Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"osSearchEngineLabel", IDS_OS_SETTINGS_SEARCH_ENGINE_LABEL},
      {"searchGoogleAssistant", IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT},
      {"searchGoogleAssistantEnabled",
       IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_ENABLED},
      {"searchGoogleAssistantDisabled",
       IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_DISABLED},
      {"searchGoogleAssistantOn", IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_ON},
      {"searchGoogleAssistantOff", IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_OFF},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  // NOTE: This will be false when the flag is disabled.
  const bool is_assistant_allowed =
      ::assistant::IsAssistantAllowedForProfile(profile) ==
      ash::mojom::AssistantAllowedState::ALLOWED;
  html_source->AddBoolean("isAssistantAllowed", is_assistant_allowed);
  html_source->AddLocalizedString("osSearchPageTitle",
                                  is_assistant_allowed
                                      ? IDS_SETTINGS_SEARCH_AND_ASSISTANT
                                      : IDS_SETTINGS_SEARCH);
  html_source->AddString("searchExplanation",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_SEARCH_EXPLANATION,
                             base::ASCIIToUTF16(chrome::kOmniboxLearnMoreURL)));
  html_source->AddString(
      "osSearchEngineTooltip",
      ui::SubstituteChromeOSDeviceType(IDS_OS_SETTINGS_SEARCH_ENGINE_TOOLTIP));
}

void AddPrivacyStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"privacyPageTitle", IDS_SETTINGS_PRIVACY},
      {"enableLogging", IDS_SETTINGS_ENABLE_LOGGING_PREF},
      {"enableLoggingDesc", IDS_SETTINGS_ENABLE_LOGGING_PREF_DESC},
      {"wakeOnWifi", IDS_SETTINGS_WAKE_ON_WIFI_DESCRIPTION},
      {"enableContentProtectionAttestation",
       IDS_SETTINGS_ENABLE_CONTENT_PROTECTION_ATTESTATION},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString("syncAndGoogleServicesLearnMoreURL",
                         chrome::kSyncAndGoogleServicesLearnMoreURL);
  ::settings::AddPersonalizationOptionsStrings(html_source);
}

void AddPeoplePageStrings(content::WebUIDataSource* html_source,
                          Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"osPeoplePageTitle", IDS_OS_SETTINGS_PEOPLE},
      {"accountManagerSubMenuLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_SUBMENU_LABEL},
      {"accountManagerPageTitle", IDS_SETTINGS_ACCOUNT_MANAGER_PAGE_TITLE},
      {"kerberosAccountsSubMenuLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_SUBMENU_LABEL},
      {"accountManagerPageTitle", IDS_SETTINGS_ACCOUNT_MANAGER_PAGE_TITLE},
      {"kerberosAccountsPageTitle", IDS_SETTINGS_KERBEROS_ACCOUNTS_PAGE_TITLE},
      {"lockScreenFingerprintTitle",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_SUBPAGE_TITLE},
      {"manageOtherPeople", IDS_SETTINGS_PEOPLE_MANAGE_OTHER_PEOPLE},
      {"osSyncPageTitle", IDS_OS_SETTINGS_SYNC_PAGE_TITLE},
      {"syncAndNonPersonalizedServices",
       IDS_SETTINGS_SYNC_SYNC_AND_NON_PERSONALIZED_SERVICES},
      {"syncDisconnectConfirm", IDS_SETTINGS_SYNC_DISCONNECT_CONFIRM},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  // Toggles the Chrome OS Account Manager submenu in the People section.
  html_source->AddBoolean("isAccountManagerEnabled",
                          chromeos::IsAccountManagerAvailable(profile));

  if (chromeos::features::IsSplitSyncConsentEnabled()) {
    static constexpr webui::LocalizedString kTurnOffStrings[] = {
        {"syncDisconnect", IDS_SETTINGS_PEOPLE_SYNC_TURN_OFF},
        {"syncDisconnectTitle",
         IDS_SETTINGS_TURN_OFF_SYNC_AND_SIGN_OUT_DIALOG_TITLE},
    };
    AddLocalizedStringsBulk(html_source, kTurnOffStrings);
  } else {
    static constexpr webui::LocalizedString kSignOutStrings[] = {
        {"syncDisconnect", IDS_SETTINGS_PEOPLE_SIGN_OUT},
        {"syncDisconnectTitle", IDS_SETTINGS_SYNC_DISCONNECT_TITLE},
    };
    AddLocalizedStringsBulk(html_source, kSignOutStrings);
  }

  std::string sync_dashboard_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kSyncGoogleDashboardURL),
          g_browser_process->GetApplicationLocale())
          .spec();

  html_source->AddString(
      "syncDisconnectExplanation",
      l10n_util::GetStringFUTF8(IDS_SETTINGS_SYNC_DISCONNECT_EXPLANATION,
                                base::ASCIIToUTF16(sync_dashboard_url)));

  AddAccountManagerPageStrings(html_source);
  AddKerberosAccountsPageStrings(html_source);
  AddKerberosAddAccountDialogStrings(html_source);
  AddLockScreenPageStrings(html_source);
  AddFingerprintListStrings(html_source);
  AddFingerprintStrings(html_source);
  AddSetupFingerprintDialogStrings(html_source);
  AddSetupPinDialogStrings(html_source);
  AddSyncControlsStrings(html_source);

  ::settings::AddSyncControlsStrings(html_source);
  ::settings::AddSyncAccountControlStrings(html_source);
  ::settings::AddPasswordPromptDialogStrings(html_source);
  ::settings::AddSyncPageStrings(html_source);
}

void AddPageVisibilityStrings(content::WebUIDataSource* html_source) {
  PrefService* local_state = g_browser_process->local_state();
  // Toggles the Chrome OS Kerberos Accounts submenu in the People section.
  // Note that the handler is also dependent on this pref.
  html_source->AddBoolean("isKerberosEnabled",
                          local_state->GetBoolean(prefs::kKerberosEnabled));
}

}  // namespace

OsSettingsLocalizedStringsProvider::OsSettingsLocalizedStringsProvider(
    Profile* profile,
    local_search_service::mojom::LocalSearchService* local_search_service) {
  local_search_service->GetIndex(
      local_search_service::mojom::LocalSearchService::IndexId::CROS_SETTINGS,
      index_remote_.BindNewPipeAndPassReceiver());

  // Add per-page string providers.
  // TODO(khorimoto): Add providers for the remaining pages.
  per_page_providers_.push_back(
      std::make_unique<InternetStringsProvider>(profile, /*delegate=*/this));
}

OsSettingsLocalizedStringsProvider::~OsSettingsLocalizedStringsProvider() =
    default;

void OsSettingsLocalizedStringsProvider::AddOsLocalizedStrings(
    content::WebUIDataSource* html_source,
    Profile* profile) {
  for (const auto& per_page_provider : per_page_providers_)
    per_page_provider->AddUiStrings(html_source);

  // TODO(khorimoto): Migrate these to OsSettingsPerPageStringsProvider
  // instances.
  AddAboutStrings(html_source, profile);
  AddA11yStrings(html_source);
  AddAndroidAppStrings(html_source);
  AddAppManagementStrings(html_source);
  AddAppsStrings(html_source);
  AddBluetoothStrings(html_source);
  AddChromeOSUserStrings(html_source, profile);
  AddCommonStrings(html_source, profile);
  AddCrostiniStrings(html_source, profile);
  AddDateTimeStrings(html_source);
  AddDeviceStrings(html_source);
  AddFilesStrings(html_source);
  AddGoogleAssistantStrings(html_source, profile);
  AddLanguagesStrings(html_source);
  AddMultideviceStrings(html_source);
  AddParentalControlStrings(html_source, profile);
  AddPageVisibilityStrings(html_source);
  AddPeoplePageStrings(html_source, profile);
  AddPersonalizationStrings(html_source);
  AddPluginVmStrings(html_source, profile);
  AddPrintingStrings(html_source);
  AddPrivacyStrings(html_source);
  AddResetStrings(html_source);
  AddSearchInSettingsStrings(html_source);
  AddSearchStrings(html_source, profile);
  AddUsersStrings(html_source);

  policy_indicator::AddLocalizedStrings(html_source);

  html_source->UseStringsJs();
}

const SearchConcept*
OsSettingsLocalizedStringsProvider::GetCanonicalTagMetadata(
    int canonical_message_id) const {
  const auto it = canonical_id_to_metadata_map_.find(canonical_message_id);
  if (it == canonical_id_to_metadata_map_.end())
    return nullptr;
  return it->second;
}

void OsSettingsLocalizedStringsProvider::Shutdown() {
  index_remote_.reset();
}

void OsSettingsLocalizedStringsProvider::AddSearchTags(
    const std::vector<SearchConcept>& tags_group) {
  index_remote_->AddOrUpdate(ConceptVectorToDataPtrVector(tags_group),
                             /*callback=*/base::DoNothing());

  // Add each concept to the map. Note that it is safe to take the address of
  // each concept because all concepts are allocated via static
  // base::NoDestructor objects in the Get*SearchConcepts() helper functions.
  for (const auto& concept : tags_group)
    canonical_id_to_metadata_map_[concept.canonical_message_id] = &concept;
}

void OsSettingsLocalizedStringsProvider::RemoveSearchTags(
    const std::vector<SearchConcept>& tags_group) {
  std::vector<std::string> ids;
  for (const auto& concept : tags_group) {
    canonical_id_to_metadata_map_.erase(concept.canonical_message_id);
    ids.push_back(base::NumberToString(concept.canonical_message_id));
  }

  index_remote_->Delete(ids, /*callback=*/base::DoNothing());
}

}  // namespace settings
}  // namespace chromeos
