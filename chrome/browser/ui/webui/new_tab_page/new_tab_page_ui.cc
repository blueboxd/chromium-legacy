// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "chrome/grit/new_tab_page_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserContext;
using content::WebContents;

namespace {

constexpr char kGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/new_tab_page/";

content::WebUIDataSource* CreateNewTabPageUiHtmlSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINewTabPageHost);

  ui::Accelerator undo_accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("undoDescription", l10n_util::GetStringFUTF16(
                                           IDS_UNDO_DESCRIPTION,
                                           undo_accelerator.GetShortcutText()));

  static constexpr webui::LocalizedString kStrings[] = {
      {"title", IDS_NEW_TAB_TITLE},
      {"undo", IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE},

      // Custom Links
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
      {"nameField", IDS_NTP_CUSTOM_LINKS_NAME},
      {"restoreDefaultLinks", IDS_NTP_CONFIRM_MSG_RESTORE_DEFAULTS},
      {"restoreThumbnailsShort", IDS_NEW_TAB_RESTORE_THUMBNAILS_SHORT_LINK},
      {"urlField", IDS_NTP_CUSTOM_LINKS_URL},

      // Customize button and dialog.
      {"backgroundsMenuItem", IDS_NTP_CUSTOMIZE_MENU_BACKGROUND_LABEL},
      {"cancelButton", IDS_CANCEL},
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"customizeButton", IDS_NTP_CUSTOMIZE_BUTTON_LABEL},
      {"customizeThisPage", IDS_NTP_CUSTOM_BG_CUSTOMIZE_NTP_LABEL},
      {"defaultThemeLabel", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"doneButton", IDS_DONE},
      {"shortcutsMenuItem", IDS_NTP_CUSTOMIZE_MENU_SHORTCUTS_LABEL},
      {"themesMenuItem", IDS_NTP_CUSTOMIZE_MENU_COLOR_LABEL},
      {"thirdPartyThemeDescription", IDS_NTP_CUSTOMIZE_3PT_THEME_DESC},
      {"uninstallThirdPartyThemeButton", IDS_NTP_CUSTOMIZE_3PT_THEME_UNINSTALL},
      {"hideShortcuts", IDS_NTP_CUSTOMIZE_HIDE_SHORTCUTS_LABEL},
      {"hideShortcutsDesc", IDS_NTP_CUSTOMIZE_HIDE_SHORTCUTS_DESC},
      {"mostVisited", IDS_NTP_CUSTOMIZE_MOST_VISITED_LABEL},
      {"myShortcuts", IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_LABEL},
      {"shortcutsCurated", IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_DESC},
      {"shortcutsOption", IDS_NTP_CUSTOMIZE_MENU_SHORTCUTS_LABEL},
      {"shortcutsSuggested", IDS_NTP_CUSTOMIZE_MOST_VISITED_DESC},
  };
  AddLocalizedStringsBulk(source, kStrings);

  source->AddResourcePath("skcolor.mojom-lite.js",
                          IDR_NEW_TAB_PAGE_SKCOLOR_MOJO_LITE_JS);
  source->AddResourcePath("new_tab_page.mojom-lite.js",
                          IDR_NEW_TAB_PAGE_MOJO_LITE_JS);
  webui::SetupWebUIDataSource(
      source, base::make_span(kNewTabPageResources, kNewTabPageResourcesSize),
      kGeneratedPath, IDR_NEW_TAB_PAGE_NEW_TAB_PAGE_HTML);

  return source;
}

}  // namespace

NewTabPageUI::NewTabPageUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true),
      page_factory_receiver_(this),
      profile_(Profile::FromWebUI(web_ui)),
      instant_service_(InstantServiceFactory::GetForProfile(profile_)) {
  content::WebUIDataSource::Add(profile_, CreateNewTabPageUiHtmlSource());

  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFavicon2));

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
      std::move(pending_page_handler), std::move(pending_page), profile_);
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
