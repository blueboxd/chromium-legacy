// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search/omnibox_mojo_utils.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/untrusted_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "chrome/grit/new_tab_page_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"

using content::BrowserContext;
using content::WebContents;

namespace {

constexpr char kGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/new_tab_page/";

content::WebUIDataSource* CreateNewTabPageUiHtmlSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINewTabPageHost);

  ui::Accelerator undo_accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("undoDescription", l10n_util::GetStringFUTF16(
                                           IDS_UNDO_DESCRIPTION,
                                           undo_accelerator.GetShortcutText()));
  source->AddString("googleBaseUrl",
                    GURL(TemplateURLServiceFactory::GetForProfile(profile)
                             ->search_terms_data()
                             .GoogleBaseURLValue())
                        .spec());

  // Realbox.
  const bool realbox_enabled =
      ntp_features::IsRealboxEnabled() &&
      base::FeatureList::IsEnabled(ntp_features::kWebUIRealbox);
  source->AddBoolean("realboxEnabled", realbox_enabled);
  source->AddBoolean("suggestionTransparencyEnabled",
                     base::FeatureList::IsEnabled(
                         omnibox::kOmniboxSuggestionTransparencyOptions));
  source->AddBoolean(
      "realboxMatchOmniboxTheme",
      base::FeatureList::IsEnabled(ntp_features::kRealboxMatchOmniboxTheme));
  source->AddString(
      "realboxDefaultIcon",
      base::FeatureList::IsEnabled(ntp_features::kRealboxUseGoogleGIcon)
          ? omnibox::kGoogleGIconResourceName
          : omnibox::kSearchIconResourceName);

  source->AddBoolean(
      "iframeOneGoogleBarEnabled",
      base::FeatureList::IsEnabled(ntp_features::kIframeOneGoogleBar));

  static constexpr webui::LocalizedString kStrings[] = {
      {"doneButton", IDS_DONE},
      {"title", IDS_NEW_TAB_TITLE},
      {"undo", IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE},

      // Custom Links.
      {"addLinkTitle", IDS_NTP_CUSTOM_LINKS_ADD_SHORTCUT_TITLE},
      {"editLinkTitle", IDS_NTP_CUSTOM_LINKS_EDIT_SHORTCUT},
      {"invalidUrl", IDS_NTP_CUSTOM_LINKS_INVALID_URL},
      {"linkAddedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_ADDED},
      {"linkCancel", IDS_NTP_CUSTOM_LINKS_CANCEL},
      {"linkCantCreate", IDS_NTP_CUSTOM_LINKS_CANT_CREATE},
      {"linkCantEdit", IDS_NTP_CUSTOM_LINKS_CANT_EDIT},
      {"linkDone", IDS_NTP_CUSTOM_LINKS_DONE},
      {"linkEditedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_EDITED},
      {"linkRemove", IDS_NTP_CUSTOM_LINKS_REMOVE},
      {"linkRemovedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_REMOVED},
      {"moreActions", IDS_SETTINGS_MORE_ACTIONS},
      {"nameField", IDS_NTP_CUSTOM_LINKS_NAME},
      {"restoreDefaultLinks", IDS_NTP_CONFIRM_MSG_RESTORE_DEFAULTS},
      {"restoreThumbnailsShort", IDS_NEW_TAB_RESTORE_THUMBNAILS_SHORT_LINK},
      {"urlField", IDS_NTP_CUSTOM_LINKS_URL},

      // Customize button and dialog.
      {"backButton", IDS_ACCNAME_BACK},
      {"backgroundsMenuItem", IDS_NTP_CUSTOMIZE_MENU_BACKGROUND_LABEL},
      {"cancelButton", IDS_CANCEL},
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"customizeButton", IDS_NTP_CUSTOMIZE_BUTTON_LABEL},
      {"customizeThisPage", IDS_NTP_CUSTOM_BG_CUSTOMIZE_NTP_LABEL},
      {"defaultThemeLabel", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"hideShortcuts", IDS_NTP_CUSTOMIZE_HIDE_SHORTCUTS_LABEL},
      {"hideShortcutsDesc", IDS_NTP_CUSTOMIZE_HIDE_SHORTCUTS_DESC},
      {"mostVisited", IDS_NTP_CUSTOMIZE_MOST_VISITED_LABEL},
      {"myShortcuts", IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_LABEL},
      {"noBackground", IDS_NTP_CUSTOMIZE_NO_BACKGROUND_LABEL},
      {"refreshDaily", IDS_NTP_CUSTOM_BG_DAILY_REFRESH},
      {"shortcutsCurated", IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_DESC},
      {"shortcutsMenuItem", IDS_NTP_CUSTOMIZE_MENU_SHORTCUTS_LABEL},
      {"shortcutsOption", IDS_NTP_CUSTOMIZE_MENU_SHORTCUTS_LABEL},
      {"shortcutsSuggested", IDS_NTP_CUSTOMIZE_MOST_VISITED_DESC},
      {"themesMenuItem", IDS_NTP_CUSTOMIZE_MENU_COLOR_LABEL},
      {"thirdPartyThemeDescription", IDS_NTP_CUSTOMIZE_3PT_THEME_DESC},
      {"uninstallThirdPartyThemeButton", IDS_NTP_CUSTOMIZE_3PT_THEME_UNINSTALL},
      {"uploadFromDevice", IDS_NTP_CUSTOMIZE_UPLOAD_FROM_DEVICE_LABEL},

      // Voice search.
      {"audioError", IDS_NEW_TAB_VOICE_AUDIO_ERROR},
      {"close", IDS_NEW_TAB_VOICE_CLOSE_TOOLTIP},
      {"details", IDS_NEW_TAB_VOICE_DETAILS},
      {"languageError", IDS_NEW_TAB_VOICE_LANGUAGE_ERROR},
      {"learnMore", IDS_LEARN_MORE},
      {"listening", IDS_NEW_TAB_VOICE_LISTENING},
      {"networkError", IDS_NEW_TAB_VOICE_NETWORK_ERROR},
      {"noTranslation", IDS_NEW_TAB_VOICE_NO_TRANSLATION},
      {"noVoice", IDS_NEW_TAB_VOICE_NO_VOICE},
      {"otherError", IDS_NEW_TAB_VOICE_OTHER_ERROR},
      {"permissionError", IDS_NEW_TAB_VOICE_PERMISSION_ERROR},
      {"speak", IDS_NEW_TAB_VOICE_READY},
      {"tryAgain", IDS_NEW_TAB_VOICE_TRY_AGAIN},
      {"voiceSearchButtonLabel", IDS_TOOLTIP_MIC_SEARCH},
      {"waiting", IDS_NEW_TAB_VOICE_WAITING},

      // Realbox.
      {"searchBoxHint", IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MD},
      {"realboxSeparator", IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR},
      {"removeSuggestion", IDS_OMNIBOX_REMOVE_SUGGESTION},

      // Logo/doodle.
      {"copyLink", IDS_NTP_DOODLE_SHARE_DIALOG_COPY_LABEL},
      {"doodleLink", IDS_NTP_DOODLE_SHARE_DIALOG_LINK_LABEL},
      {"email", IDS_NTP_DOODLE_SHARE_DIALOG_MAIL_LABEL},
      {"facebook", IDS_NTP_DOODLE_SHARE_DIALOG_FACEBOOK_LABEL},
      {"shareDoodle", IDS_NTP_DOODLE_SHARE_LABEL},
      {"twitter", IDS_NTP_DOODLE_SHARE_DIALOG_TWITTER_LABEL},
  };
  AddLocalizedStringsBulk(source, kStrings);

  // Register images that are purposefully not inlined in the HTML and instead
  // are set in Javascript.
  static constexpr webui::ResourcePath kImages[] = {
      {omnibox::kGoogleGIconResourceName, IDR_WEBUI_IMAGES_200_LOGO_GOOGLE_G},
      {omnibox::kBookmarkIconResourceName, IDR_LOCAL_NTP_ICONS_BOOKMARK},
      {omnibox::kCalculatorIconResourceName, IDR_LOCAL_NTP_ICONS_CALCULATOR},
      {omnibox::kClockIconResourceName, IDR_LOCAL_NTP_ICONS_CLOCK},
      {omnibox::kDriveDocsIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_DOCS},
      {omnibox::kDriveFolderIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_FOLDER},
      {omnibox::kDriveFormIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_FORM},
      {omnibox::kDriveImageIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_IMAGE},
      {omnibox::kDriveLogoIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_LOGO},
      {omnibox::kDrivePdfIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_PDF},
      {omnibox::kDriveSheetsIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_SHEETS},
      {omnibox::kDriveSlidesIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_SLIDES},
      {omnibox::kDriveVideoIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_VIDEO},
      {omnibox::kExtensionAppIconResourceName,
       IDR_LOCAL_NTP_ICONS_EXTENSION_APP},
      {omnibox::kPageIconResourceName, IDR_LOCAL_NTP_ICONS_PAGE},
      {omnibox::kSearchIconResourceName, IDR_WEBUI_IMAGES_ICON_SEARCH}};
  webui::AddResourcePathsBulk(source, kImages);

  source->AddResourcePath("skcolor.mojom-lite.js",
                          IDR_NEW_TAB_PAGE_SKCOLOR_MOJO_LITE_JS);
  source->AddResourcePath("new_tab_page.mojom-lite.js",
                          IDR_NEW_TAB_PAGE_MOJO_LITE_JS);
  source->AddResourcePath("omnibox.mojom-lite.js",
                          IDR_NEW_TAB_PAGE_OMNIBOX_MOJO_LITE_JS);
#if BUILDFLAG(OPTIMIZE_WEBUI)
  source->AddResourcePath("new_tab_page.js", IDR_NEW_TAB_PAGE_NEW_TAB_PAGE_JS);
#endif  // BUILDFLAG(OPTIMIZE_WEBUI)
  webui::SetupWebUIDataSource(
      source, base::make_span(kNewTabPageResources, kNewTabPageResourcesSize),
      kGeneratedPath, IDR_NEW_TAB_PAGE_NEW_TAB_PAGE_HTML);

  // Allows creating <script> and inlining as well as network requests to
  // support inlining the OneGoogleBar.
  // TODO(crbug.com/1076506): remove when changing to iframed OneGoogleBar.
  // Needs to happen after |webui::SetupWebUIDataSource()| since also overrides
  // script-src.
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources chrome://test 'self' 'unsafe-inline' "
      "https:;");
  // Allow embedding of iframes from the One Google Bar and
  // chrome-untrusted://new-tab-page for other external content and resources.
  source->OverrideContentSecurityPolicyChildSrc(
      base::StringPrintf("child-src https://*.google.com/ %s %s;",
                         google_util::CommandLineGoogleBaseURL().spec().c_str(),
                         chrome::kChromeUIUntrustedNewTabPageUrl));

  return source;
}

}  // namespace

NewTabPageUI::NewTabPageUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true),
      page_factory_receiver_(this),
      profile_(Profile::FromWebUI(web_ui)),
      instant_service_(InstantServiceFactory::GetForProfile(profile_)),
      web_contents_(web_ui->GetWebContents()),
      creation_time_(base::Time::Now()) {
  content::WebUIDataSource::Add(profile_,
                                CreateNewTabPageUiHtmlSource(profile_));

  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFavicon2));
  content::URLDataSource::Add(profile_,
                              std::make_unique<UntrustedSource>(profile_));

  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  UpdateBackgroundColor(*instant_service_->GetInitializedNtpTheme());
  instant_service_->AddObserver(this);
}

WEB_UI_CONTROLLER_TYPE_IMPL(NewTabPageUI)

NewTabPageUI::~NewTabPageUI() {
  instant_service_->RemoveObserver(this);
}

// static
bool NewTabPageUI::IsNewTabPageOrigin(const GURL& url) {
  return url.GetOrigin() == GURL(chrome::kChromeUINewTabPageURL).GetOrigin();
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }

  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageUI::CreatePageHandler(
    mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
    mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());
  page_handler_ = std::make_unique<NewTabPageHandler>(
      std::move(pending_page_handler), std::move(pending_page), profile_,
      web_contents_, creation_time_);
}

void NewTabPageUI::NtpThemeChanged(const NtpTheme& theme) {
  // Load time data is cached across page reloads. Update the background color
  // here to prevent a white flicker on page reload.
  UpdateBackgroundColor(theme);
}

void NewTabPageUI::MostVisitedInfoChanged(const InstantMostVisitedInfo& info) {}

void NewTabPageUI::UpdateBackgroundColor(const NtpTheme& theme) {
  std::unique_ptr<base::DictionaryValue> update(new base::DictionaryValue);
  auto background_color = theme.background_color;
  update->SetString(
      "backgroundColor",
      base::StringPrintf("#%02X%02X%02X", SkColorGetR(background_color),
                         SkColorGetG(background_color),
                         SkColorGetB(background_color)));
  content::WebUIDataSource::Update(profile_, chrome::kChromeUINewTabPageHost,
                                   std::move(update));
}
