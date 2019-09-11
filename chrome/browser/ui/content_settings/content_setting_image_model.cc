// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/permissions/permission_features.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model_states.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/device_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

#if defined(OS_MACOSX)
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif

using content::WebContents;

// The image models hierarchy:
//
// ContentSettingImageModel                  - base class
//   ContentSettingSimpleImageModel            - single content setting
//     ContentSettingBlockedImageModel           - generic blocked setting
//     ContentSettingGeolocationImageModel       - geolocation
//     ContentSettingRPHImageModel               - protocol handlers
//     ContentSettingMIDISysExImageModel         - midi sysex
//     ContentSettingDownloadsImageModel         - automatic downloads
//     ContentSettingClipboardReadImageModel     - clipboard read
//     ContentSettingSensorsImageModel           - sensors
//     ContentSettingNotificationsImageModel     - notifications
//   ContentSettingMediaImageModel             - media
//   ContentSettingFramebustBlockImageModel    - blocked framebust

class ContentSettingBlockedImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingBlockedImageModel(ImageType image_type,
                                  ContentSettingsType content_type);

  bool UpdateAndGetVisibility(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingBlockedImageModel);
};

class ContentSettingGeolocationImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingGeolocationImageModel();

  bool UpdateAndGetVisibility(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingGeolocationImageModel);
};

class ContentSettingRPHImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingRPHImageModel();

  bool UpdateAndGetVisibility(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingRPHImageModel);
};

class ContentSettingMIDISysExImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingMIDISysExImageModel();

  bool UpdateAndGetVisibility(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingMIDISysExImageModel);
};

class ContentSettingDownloadsImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingDownloadsImageModel();

  bool UpdateAndGetVisibility(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingDownloadsImageModel);
};

class ContentSettingClipboardReadImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingClipboardReadImageModel();

  bool UpdateAndGetVisibility(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingClipboardReadImageModel);
};

// Image model for displaying media icons in the location bar.
class ContentSettingMediaImageModel : public ContentSettingImageModel {
 public:
  ContentSettingMediaImageModel();

  bool UpdateAndGetVisibility(WebContents* web_contents) override;
  bool IsMicAccessed();
  bool IsCamAccessed();
  bool IsMicBlockedOnSiteLevel();
  bool IsCameraBlockedOnSiteLevel();
#if defined(OS_MACOSX)
  bool DidCameraAccessFailBecauseOfSystemLevelBlock();
  bool DidMicAccessFailBecauseOfSystemLevelBlock();
#endif  // defined(OS_MACOSX)

  std::unique_ptr<ContentSettingBubbleModel> CreateBubbleModelImpl(
      ContentSettingBubbleModel::Delegate* delegate,
      WebContents* web_contents) override;

 private:
  TabSpecificContentSettings::MicrophoneCameraState state_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingMediaImageModel);
};

class ContentSettingSensorsImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingSensorsImageModel();

  bool UpdateAndGetVisibility(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingSensorsImageModel);
};

// The image model for an icon that acts as a quiet permission request prompt
// for notifications. In contrast to other icons -- which are either
// permission-in-use indicators or permission-blocked indicators -- this is
// shown before the user makes the first permission decision, and in fact,
// allows the user to make that decision.
class ContentSettingNotificationsImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingNotificationsImageModel();

  // ContentSettingSimpleImageModel:
  bool UpdateAndGetVisibility(WebContents* web_contents) override;
  std::unique_ptr<ContentSettingBubbleModel> CreateBubbleModelImpl(
      ContentSettingBubbleModel::Delegate* delegate,
      WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingNotificationsImageModel);
};

class ContentSettingPopupImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingPopupImageModel();

  bool UpdateAndGetVisibility(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingPopupImageModel);
};

namespace {

bool ShouldShowPluginExplanation(content::WebContents* web_contents,
                                 HostContentSettingsMap* map) {
  const GURL& url = web_contents->GetURL();
  ContentSetting setting = map->GetContentSetting(
      url, url, CONTENT_SETTINGS_TYPE_PLUGINS, std::string());

  // For plugins, show the animated explanation in these cases:
  //  - The plugin is blocked despite the user having content setting ALLOW.
  //  - The user has disabled Flash using BLOCK.
  return setting == CONTENT_SETTING_ALLOW || setting == CONTENT_SETTING_BLOCK;
}

struct ContentSettingsImageDetails {
  ContentSettingsType content_type;
  const gfx::VectorIcon& icon;
  int blocked_tooltip_id;
  int blocked_explanatory_text_id;
  int accessed_tooltip_id;
};

const ContentSettingsImageDetails kImageDetails[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, kCookieIcon, IDS_BLOCKED_COOKIES_MESSAGE, 0,
     IDS_ACCESSED_COOKIES_MESSAGE},
    {CONTENT_SETTINGS_TYPE_IMAGES, kPhotoIcon, IDS_BLOCKED_IMAGES_MESSAGE, 0,
     0},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, kCodeIcon,
     IDS_BLOCKED_JAVASCRIPT_MESSAGE, 0, 0},
    {CONTENT_SETTINGS_TYPE_PLUGINS, kExtensionIcon, IDS_BLOCKED_PLUGINS_MESSAGE,
     IDS_BLOCKED_PLUGIN_EXPLANATORY_TEXT, 0},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, kMixedContentIcon,
     IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT, 0, 0},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, kExtensionIcon,
     IDS_BLOCKED_PPAPI_BROKER_MESSAGE, 0, IDS_ALLOWED_PPAPI_BROKER_MESSAGE},
    {CONTENT_SETTINGS_TYPE_SOUND, kTabAudioIcon, IDS_BLOCKED_SOUND_TITLE, 0, 0},
    {CONTENT_SETTINGS_TYPE_ADS, kAdsIcon, IDS_BLOCKED_ADS_PROMPT_TOOLTIP,
     IDS_BLOCKED_ADS_PROMPT_TITLE, 0},
};

const ContentSettingsImageDetails* GetImageDetails(ContentSettingsType type) {
  for (const ContentSettingsImageDetails& image_details : kImageDetails) {
    if (image_details.content_type == type)
      return &image_details;
  }
  return nullptr;
}

}  // namespace

// Single content setting ------------------------------------------------------

ContentSettingSimpleImageModel::ContentSettingSimpleImageModel(
    ImageType image_type,
    ContentSettingsType content_type)
    : ContentSettingImageModel(image_type), content_type_(content_type) {}

std::unique_ptr<ContentSettingBubbleModel>
ContentSettingSimpleImageModel::CreateBubbleModelImpl(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents) {
  return ContentSettingBubbleModel::CreateContentSettingBubbleModel(
      delegate, web_contents, content_type());
}

// static
std::unique_ptr<ContentSettingImageModel>
ContentSettingImageModel::CreateForContentType(ImageType image_type) {
  switch (image_type) {
    case ImageType::COOKIES:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::COOKIES, CONTENT_SETTINGS_TYPE_COOKIES);
    case ImageType::IMAGES:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::IMAGES, CONTENT_SETTINGS_TYPE_IMAGES);
    case ImageType::JAVASCRIPT:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::JAVASCRIPT, CONTENT_SETTINGS_TYPE_JAVASCRIPT);
    case ImageType::PPAPI_BROKER:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::PPAPI_BROKER, CONTENT_SETTINGS_TYPE_PPAPI_BROKER);
    case ImageType::PLUGINS:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::PLUGINS, CONTENT_SETTINGS_TYPE_PLUGINS);
    case ImageType::POPUPS:
      return std::make_unique<ContentSettingPopupImageModel>();
    case ImageType::GEOLOCATION:
      return std::make_unique<ContentSettingGeolocationImageModel>();
    case ImageType::MIXEDSCRIPT:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::MIXEDSCRIPT, CONTENT_SETTINGS_TYPE_MIXEDSCRIPT);
    case ImageType::PROTOCOL_HANDLERS:
      return std::make_unique<ContentSettingRPHImageModel>();
    case ImageType::MEDIASTREAM:
      return std::make_unique<ContentSettingMediaImageModel>();
    case ImageType::ADS:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::ADS, CONTENT_SETTINGS_TYPE_ADS);
    case ImageType::AUTOMATIC_DOWNLOADS:
      return std::make_unique<ContentSettingDownloadsImageModel>();
    case ImageType::MIDI_SYSEX:
      return std::make_unique<ContentSettingMIDISysExImageModel>();
    case ImageType::SOUND:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::SOUND, CONTENT_SETTINGS_TYPE_SOUND);
    case ImageType::FRAMEBUST:
      return std::make_unique<ContentSettingFramebustBlockImageModel>();
    case ImageType::CLIPBOARD_READ:
      return std::make_unique<ContentSettingClipboardReadImageModel>();
    case ImageType::SENSORS:
      return std::make_unique<ContentSettingSensorsImageModel>();
    case ImageType::NOTIFICATIONS_QUIET_PROMPT:
      return std::make_unique<ContentSettingNotificationsImageModel>();
    case ImageType::NUM_IMAGE_TYPES:
      break;
  }
  NOTREACHED();
  return nullptr;
}

void ContentSettingImageModel::Update(content::WebContents* contents) {
  bool new_visibility = contents ? UpdateAndGetVisibility(contents) : false;
  is_visible_ = new_visibility;
  if (contents && !is_visible_) {
    ContentSettingImageModelStates::Get(contents)->SetAnimationHasRun(
        image_type(), false);
  }
}

bool ContentSettingImageModel::ShouldRunAnimation(
    content::WebContents* contents) {
  DCHECK(contents);
  return !ContentSettingImageModelStates::Get(contents)->AnimationHasRun(
      image_type());
}

void ContentSettingImageModel::SetAnimationHasRun(
    content::WebContents* contents) {
  DCHECK(contents);
  ContentSettingImageModelStates::Get(contents)->SetAnimationHasRun(
      image_type(), true);
}

// Generic blocked content settings --------------------------------------------

ContentSettingBlockedImageModel::ContentSettingBlockedImageModel(
    ImageType image_type,
    ContentSettingsType content_type)
    : ContentSettingSimpleImageModel(image_type, content_type) {}

bool ContentSettingBlockedImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  const ContentSettingsType type = content_type();
  const ContentSettingsImageDetails* image_details = GetImageDetails(type);
  DCHECK(image_details) << "No entry for " << type << " in kImageDetails[].";

  int tooltip_id = image_details->blocked_tooltip_id;
  int explanation_id = image_details->blocked_explanatory_text_id;

  // If a content type is blocked by default and was accessed, display the
  // content blocked page action.
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return false;

  bool is_blocked = content_settings->IsContentBlocked(type);
  bool is_allowed = content_settings->IsContentAllowed(type);
  if (!is_blocked && !is_allowed)
    return false;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);

  // For allowed cookies, don't show the cookie page action unless cookies are
  // blocked by default.
  if (!is_blocked && type == CONTENT_SETTINGS_TYPE_COOKIES &&
      map->GetDefaultContentSetting(type, nullptr) != CONTENT_SETTING_BLOCK) {
    return false;
  }

  if (type == CONTENT_SETTINGS_TYPE_COOKIES &&
      CookieSettingsFactory::GetForProfile(profile)
          ->IsCookieControlsEnabled()) {
    return false;
  }

  if (!is_blocked) {
    tooltip_id = image_details->accessed_tooltip_id;
    explanation_id = 0;
  }

  if (type == CONTENT_SETTINGS_TYPE_PLUGINS &&
      !ShouldShowPluginExplanation(web_contents, map)) {
    explanation_id = 0;
  }

  const gfx::VectorIcon* badge_id = &gfx::kNoneIcon;
  if (type == CONTENT_SETTINGS_TYPE_PPAPI_BROKER)
    badge_id = &kWarningBadgeIcon;
  else if (content_settings->IsContentBlocked(type))
    badge_id = &kBlockedBadgeIcon;

  const gfx::VectorIcon* icon = &image_details->icon;
  // Touch mode uses a different tab audio icon.
  if (image_details->content_type == CONTENT_SETTINGS_TYPE_SOUND &&
      ui::MaterialDesignController::touch_ui()) {
    icon = &kTabAudioRoundedIcon;
  }
  set_icon(*icon, *badge_id);
  set_explanatory_string_id(explanation_id);
  DCHECK(tooltip_id);
  set_tooltip(l10n_util::GetStringUTF16(tooltip_id));
  return true;
}

// Geolocation -----------------------------------------------------------------

ContentSettingGeolocationImageModel::ContentSettingGeolocationImageModel()
    : ContentSettingSimpleImageModel(ImageType::GEOLOCATION,
                                     CONTENT_SETTINGS_TYPE_GEOLOCATION) {}

bool ContentSettingGeolocationImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return false;
  const ContentSettingsUsagesState& usages_state =
      content_settings->geolocation_usages_state();
  if (usages_state.state_map().empty())
    return false;

  // If any embedded site has access the allowed icon takes priority over the
  // blocked icon.
  unsigned int state_flags = 0;
  usages_state.GetDetailedInfo(nullptr, &state_flags);
  bool allowed =
      !!(state_flags & ContentSettingsUsagesState::TABSTATE_HAS_ANY_ALLOWED);
  set_icon(kMyLocationIcon, allowed ? gfx::kNoneIcon : kBlockedBadgeIcon);
  set_tooltip(l10n_util::GetStringUTF16(allowed
                                            ? IDS_GEOLOCATION_ALLOWED_TOOLTIP
                                            : IDS_GEOLOCATION_BLOCKED_TOOLTIP));
  return true;
}

// Protocol handlers -----------------------------------------------------------

ContentSettingRPHImageModel::ContentSettingRPHImageModel()
    : ContentSettingSimpleImageModel(ImageType::PROTOCOL_HANDLERS,
                                     CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS) {
  set_icon(vector_icons::kProtocolHandlerIcon, gfx::kNoneIcon);
  set_tooltip(l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_TOOLTIP));
}

bool ContentSettingRPHImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return false;
  if (content_settings->pending_protocol_handler().IsEmpty())
    return false;

  return true;
}

// MIDI SysEx ------------------------------------------------------------------

ContentSettingMIDISysExImageModel::ContentSettingMIDISysExImageModel()
    : ContentSettingSimpleImageModel(ImageType::MIDI_SYSEX,
                                     CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {}

bool ContentSettingMIDISysExImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return false;
  const ContentSettingsUsagesState& usages_state =
      content_settings->midi_usages_state();
  if (usages_state.state_map().empty())
    return false;

  // If any embedded site has access the allowed icon takes priority over the
  // blocked icon.
  unsigned int state_flags = 0;
  usages_state.GetDetailedInfo(nullptr, &state_flags);
  bool allowed =
      !!(state_flags & ContentSettingsUsagesState::TABSTATE_HAS_ANY_ALLOWED);
  set_icon(vector_icons::kMidiIcon,
           allowed ? gfx::kNoneIcon : kBlockedBadgeIcon);
  set_tooltip(l10n_util::GetStringUTF16(allowed
                                            ? IDS_MIDI_SYSEX_ALLOWED_TOOLTIP
                                            : IDS_MIDI_SYSEX_BLOCKED_TOOLTIP));
  return true;
}

// Automatic downloads ---------------------------------------------------------

ContentSettingDownloadsImageModel::ContentSettingDownloadsImageModel()
    : ContentSettingSimpleImageModel(
          ImageType::AUTOMATIC_DOWNLOADS,
          CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS) {}

bool ContentSettingDownloadsImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  DownloadRequestLimiter* download_request_limiter =
      g_browser_process->download_request_limiter();

  // DownloadRequestLimiter can be absent in unit_tests.
  if (!download_request_limiter)
    return false;

  switch (download_request_limiter->GetDownloadUiStatus(web_contents)) {
    case DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED:
      set_icon(kFileDownloadIcon, gfx::kNoneIcon);
      set_explanatory_string_id(0);
      set_tooltip(l10n_util::GetStringUTF16(IDS_ALLOWED_DOWNLOAD_TITLE));
      return true;
    case DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED:
      set_icon(kFileDownloadIcon, kBlockedBadgeIcon);
      set_explanatory_string_id(IDS_BLOCKED_DOWNLOADS_EXPLANATION);
      set_tooltip(l10n_util::GetStringUTF16(IDS_BLOCKED_DOWNLOAD_TITLE));
      return true;
    case DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT:
      // No need to show icon otherwise.
      return false;
  }
}

// Clipboard -------------------------------------------------------------------

ContentSettingClipboardReadImageModel::ContentSettingClipboardReadImageModel()
    : ContentSettingSimpleImageModel(ImageType::CLIPBOARD_READ,
                                     CONTENT_SETTINGS_TYPE_CLIPBOARD_READ) {}

bool ContentSettingClipboardReadImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return false;
  ContentSettingsType content_type = CONTENT_SETTINGS_TYPE_CLIPBOARD_READ;
  bool blocked = content_settings->IsContentBlocked(content_type);
  bool allowed = content_settings->IsContentAllowed(content_type);
  if (!blocked && !allowed)
    return false;

  set_icon(kContentPasteIcon, allowed ? gfx::kNoneIcon : kBlockedBadgeIcon);
  set_tooltip(l10n_util::GetStringUTF16(
      allowed ? IDS_ALLOWED_CLIPBOARD_MESSAGE : IDS_BLOCKED_CLIPBOARD_MESSAGE));
  return true;
}

// Media -----------------------------------------------------------------------

ContentSettingMediaImageModel::ContentSettingMediaImageModel()
    : ContentSettingImageModel(ImageType::MEDIASTREAM) {}

bool ContentSettingMediaImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return false;
  state_ = content_settings->GetMicrophoneCameraState();

  // If neither the microphone nor the camera stream was accessed then no icon
  // is displayed in the omnibox.
  if (state_ == TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED)
    return false;

#if defined(OS_MACOSX)
  if (base::FeatureList::IsEnabled(
          ::features::kMacSystemMediaPermissionsInfoUi)) {
    if (DidCameraAccessFailBecauseOfSystemLevelBlock() &&
        DidMicAccessFailBecauseOfSystemLevelBlock()) {
      set_icon(vector_icons::kVideocamIcon, kBlockedBadgeIcon);
      set_explanatory_string_id(IDS_CAMERA_BLOCKED_TITLE);
      set_tooltip(l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_BLOCKED));
      return true;
    } else if (DidCameraAccessFailBecauseOfSystemLevelBlock()) {
      set_icon(vector_icons::kVideocamIcon, kBlockedBadgeIcon);
      set_explanatory_string_id(IDS_CAMERA_BLOCKED_TITLE);
      set_tooltip(l10n_util::GetStringUTF16(IDS_CAMERA_BLOCKED));
      return true;
    } else if (DidMicAccessFailBecauseOfSystemLevelBlock()) {
      set_icon(vector_icons::kMicIcon, kBlockedBadgeIcon);
      set_explanatory_string_id(IDS_MICROPHONE_BLOCKED_TITLE);
      set_tooltip(l10n_util::GetStringUTF16(IDS_MICROPHONE_BLOCKED));
      return true;
    }
  }
#endif  // defined(OS_MACOSX)

  DCHECK(IsMicAccessed() || IsCamAccessed());

  int id = IDS_CAMERA_BLOCKED;
  if (IsMicBlockedOnSiteLevel() || IsCameraBlockedOnSiteLevel()) {
    set_icon(vector_icons::kVideocamIcon, kBlockedBadgeIcon);
    if (IsMicAccessed())
      id = IsCamAccessed() ? IDS_MICROPHONE_CAMERA_BLOCKED
                           : IDS_MICROPHONE_BLOCKED;
  } else {
    set_icon(vector_icons::kVideocamIcon, gfx::kNoneIcon);
    id = IDS_CAMERA_ACCESSED;
    if (IsMicAccessed())
      id = IsCamAccessed() ? IDS_MICROPHONE_CAMERA_ALLOWED
                           : IDS_MICROPHONE_ACCESSED;
  }
  set_tooltip(l10n_util::GetStringUTF16(id));

  return true;
}

bool ContentSettingMediaImageModel::IsMicAccessed() {
  return ((state_ & TabSpecificContentSettings::MICROPHONE_ACCESSED) != 0);
}

bool ContentSettingMediaImageModel::IsCamAccessed() {
  return ((state_ & TabSpecificContentSettings::CAMERA_ACCESSED) != 0);
}

bool ContentSettingMediaImageModel::IsMicBlockedOnSiteLevel() {
  return ((state_ & TabSpecificContentSettings::MICROPHONE_BLOCKED) != 0);
}

bool ContentSettingMediaImageModel::IsCameraBlockedOnSiteLevel() {
  return ((state_ & TabSpecificContentSettings::CAMERA_BLOCKED) != 0);
}

#if defined(OS_MACOSX)
bool ContentSettingMediaImageModel::
    DidCameraAccessFailBecauseOfSystemLevelBlock() {
  return (IsCamAccessed() && !IsCameraBlockedOnSiteLevel() &&
          system_media_permissions::CheckSystemVideoCapturePermission() ==
              system_media_permissions::SystemPermission::kDenied);
}

bool ContentSettingMediaImageModel::
    DidMicAccessFailBecauseOfSystemLevelBlock() {
  return (IsMicAccessed() && !IsMicBlockedOnSiteLevel() &&
          system_media_permissions::CheckSystemAudioCapturePermission() ==
              system_media_permissions::SystemPermission::kDenied);
}
#endif  // defined(OS_MACOSX)

std::unique_ptr<ContentSettingBubbleModel>
ContentSettingMediaImageModel::CreateBubbleModelImpl(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents) {
  return std::make_unique<ContentSettingMediaStreamBubbleModel>(delegate,
                                                                web_contents);
}

// Blocked Framebust -----------------------------------------------------------
ContentSettingFramebustBlockImageModel::ContentSettingFramebustBlockImageModel()
    : ContentSettingImageModel(ImageType::FRAMEBUST) {}

bool ContentSettingFramebustBlockImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  // Early exit if no blocked Framebust.
  if (!FramebustBlockTabHelper::FromWebContents(web_contents)->HasBlockedUrls())
    return false;

  set_icon(kBlockedRedirectIcon, kBlockedBadgeIcon);
  set_explanatory_string_id(IDS_REDIRECT_BLOCKED_TITLE);
  set_tooltip(l10n_util::GetStringUTF16(IDS_REDIRECT_BLOCKED_TOOLTIP));
  return true;
}

std::unique_ptr<ContentSettingBubbleModel>
ContentSettingFramebustBlockImageModel::CreateBubbleModelImpl(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents) {
  return std::make_unique<ContentSettingFramebustBlockBubbleModel>(
      delegate, web_contents);
}

// Sensors ---------------------------------------------------------------------

ContentSettingSensorsImageModel::ContentSettingSensorsImageModel()
    : ContentSettingSimpleImageModel(ImageType::SENSORS,
                                     CONTENT_SETTINGS_TYPE_SENSORS) {}

bool ContentSettingSensorsImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  auto* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return false;

  bool blocked = content_settings->IsContentBlocked(content_type());
  bool allowed = content_settings->IsContentAllowed(content_type());
  if (!blocked && !allowed)
    return false;

  HostContentSettingsMap* map = HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));

  // Do not show any indicator if sensors are allowed by default and they were
  // not blocked in this page.
  if (!blocked && map->GetDefaultContentSetting(content_type(), nullptr) ==
                      CONTENT_SETTING_ALLOW) {
    return false;
  }

  set_icon(kSensorsIcon, !blocked ? gfx::kNoneIcon : kBlockedBadgeIcon);
  if (base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)) {
    set_tooltip(l10n_util::GetStringUTF16(
        !blocked ? IDS_SENSORS_ALLOWED_TOOLTIP : IDS_SENSORS_BLOCKED_TOOLTIP));
  } else {
    set_tooltip(l10n_util::GetStringUTF16(
        !blocked ? IDS_MOTION_SENSORS_ALLOWED_TOOLTIP
                 : IDS_MOTION_SENSORS_BLOCKED_TOOLTIP));
  }
  return true;
}

// Popups ---------------------------------------------------------------------

ContentSettingPopupImageModel::ContentSettingPopupImageModel()
    : ContentSettingSimpleImageModel(ImageType::POPUPS,
                                     CONTENT_SETTINGS_TYPE_POPUPS) {}

bool ContentSettingPopupImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings || !content_settings->IsContentBlocked(content_type()))
    return false;
  set_icon(kWebIcon, kBlockedBadgeIcon);
  set_explanatory_string_id(IDS_BLOCKED_POPUPS_EXPLANATORY_TEXT);
  set_tooltip(l10n_util::GetStringUTF16(IDS_BLOCKED_POPUPS_TOOLTIP));
  return true;
}

// Notifications --------------------------------------------------------------

ContentSettingNotificationsImageModel::ContentSettingNotificationsImageModel()
    : ContentSettingSimpleImageModel(ImageType::NOTIFICATIONS_QUIET_PROMPT,
                                     CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
  set_icon(vector_icons::kNotificationsOffIcon, gfx::kNoneIcon);
  set_tooltip(
      l10n_util::GetStringUTF16(IDS_NOTIFICATIONS_OFF_EXPLANATORY_TEXT));
  if (QuietNotificationsPromptConfig::UIFlavorToUse() ==
      QuietNotificationsPromptConfig::UIFlavor::ANIMATED_ICON) {
    set_explanatory_string_id(IDS_NOTIFICATIONS_OFF_EXPLANATORY_TEXT);
  }
}

bool ContentSettingNotificationsImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  auto* manager = PermissionRequestManager::FromWebContents(web_contents);
  // |manager| may be null in tests.
  return manager ? manager->ShouldShowQuietPermissionPrompt() : false;
}

std::unique_ptr<ContentSettingBubbleModel>
ContentSettingNotificationsImageModel::CreateBubbleModelImpl(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents) {
  return std::make_unique<ContentSettingNotificationsBubbleModel>(delegate,
                                                                  web_contents);
}

// Base class ------------------------------------------------------------------

gfx::Image ContentSettingImageModel::GetIcon(SkColor icon_color) const {
  int icon_size = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
  return gfx::Image(gfx::CreateVectorIconWithBadge(*icon_, icon_size,
                                                   icon_color, *icon_badge_));
}

ContentSettingImageModel::ContentSettingImageModel(ImageType image_type)
    : is_visible_(false),
      icon_(&gfx::kNoneIcon),
      icon_badge_(&gfx::kNoneIcon),
      explanatory_string_id_(0),
      image_type_(image_type) {}

std::unique_ptr<ContentSettingBubbleModel>
ContentSettingImageModel::CreateBubbleModel(
    ContentSettingBubbleModel::Delegate* delegate,
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.ImagePressed", image_type(),
      ContentSettingImageModel::ImageType::NUM_IMAGE_TYPES);
  return CreateBubbleModelImpl(delegate, web_contents);
}

// static
std::vector<std::unique_ptr<ContentSettingImageModel>>
ContentSettingImageModel::GenerateContentSettingImageModels() {
  // The ordering of the models here influences the order in which icons are
  // shown in the omnibox.
  constexpr ImageType kContentSettingImageOrder[] = {
      ImageType::COOKIES,
      ImageType::IMAGES,
      ImageType::JAVASCRIPT,
      ImageType::PPAPI_BROKER,
      ImageType::PLUGINS,
      ImageType::POPUPS,
      ImageType::GEOLOCATION,
      ImageType::MIXEDSCRIPT,
      ImageType::PROTOCOL_HANDLERS,
      ImageType::MEDIASTREAM,
      ImageType::SENSORS,
      ImageType::ADS,
      ImageType::AUTOMATIC_DOWNLOADS,
      ImageType::MIDI_SYSEX,
      ImageType::SOUND,
      ImageType::FRAMEBUST,
      ImageType::CLIPBOARD_READ,
      ImageType::NOTIFICATIONS_QUIET_PROMPT,
  };

  std::vector<std::unique_ptr<ContentSettingImageModel>> result;
  for (auto type : kContentSettingImageOrder)
    result.push_back(CreateForContentType(type));

  return result;
}

// static
size_t ContentSettingImageModel::GetContentSettingImageModelIndexForTesting(
    ImageType image_type) {
  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      GenerateContentSettingImageModels();
  for (size_t i = 0; i < models.size(); ++i) {
    if (image_type == models[i]->image_type())
      return i;
  }
  NOTREACHED();
  return models.size();
}
