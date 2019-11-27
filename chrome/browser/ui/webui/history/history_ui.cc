// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/history/browsing_history_handler.h"
#include "chrome/browser/ui/webui/history/foreign_session_handler.h"
#include "chrome/browser/ui/webui/history/history_login_handler.h"
#include "chrome/browser/ui/webui/history/navigation_handler.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/history_resources.h"
#include "chrome/grit/history_resources_map.h"
#include "chrome/grit/locale_settings.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kIsUserSignedInKey[] = "isUserSignedIn";
constexpr char kShowMenuPromoKey[] = "showMenuPromo";

bool IsUserSignedIn(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return identity_manager && identity_manager->HasPrimaryAccount();
}

bool MenuPromoShown(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kHistoryMenuPromoShown);
}

content::WebUIDataSource* CreateHistoryUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIHistoryHost);

  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order).
      {"actionMenuDescription", IDS_HISTORY_ACTION_MENU_DESCRIPTION},
      {"ariaRoleDescription", IDS_HISTORY_ARIA_ROLE_DESCRIPTION},
      {"bookmarked", IDS_HISTORY_ENTRY_BOOKMARKED},
      {"cancel", IDS_CANCEL},
      {"clearBrowsingData", IDS_CLEAR_BROWSING_DATA_TITLE},
      {"clearSearch", IDS_HISTORY_CLEAR_SEARCH},
      {"closeMenuPromo", IDS_HISTORY_CLOSE_MENU_PROMO},
      {"collapseSessionButton", IDS_HISTORY_OTHER_SESSIONS_COLLAPSE_SESSION},
      {"delete", IDS_HISTORY_DELETE},
      {"deleteConfirm", IDS_HISTORY_DELETE_PRIOR_VISITS_CONFIRM_BUTTON},
      {"deleteSession", IDS_HISTORY_OTHER_SESSIONS_HIDE_FOR_NOW},
      {"deleteWarning", IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING},
      {"entrySummary", IDS_HISTORY_ENTRY_SUMMARY},
      {"expandSessionButton", IDS_HISTORY_OTHER_SESSIONS_EXPAND_SESSION},
      {"foundSearchResults", IDS_HISTORY_FOUND_SEARCH_RESULTS},
      {"historyMenuButton", IDS_HISTORY_HISTORY_MENU_DESCRIPTION},
      {"historyMenuItem", IDS_HISTORY_HISTORY_MENU_ITEM},
      {"itemsSelected", IDS_HISTORY_ITEMS_SELECTED},
      {"loading", IDS_HISTORY_LOADING},
      {"menu", IDS_MENU},
      {"menuPromo", IDS_HISTORY_MENU_PROMO},
      {"moreFromSite", IDS_HISTORY_MORE_FROM_SITE},
      {"openAll", IDS_HISTORY_OTHER_SESSIONS_OPEN_ALL},
      {"openTabsMenuItem", IDS_HISTORY_OPEN_TABS_MENU_ITEM},
      {"noResults", IDS_HISTORY_NO_RESULTS},
      {"noSearchResults", IDS_HISTORY_NO_SEARCH_RESULTS},
      {"noSyncedResults", IDS_HISTORY_NO_SYNCED_RESULTS},
      {"removeBookmark", IDS_HISTORY_REMOVE_BOOKMARK},
      {"removeFromHistory", IDS_HISTORY_REMOVE_PAGE},
      {"removeSelected", IDS_HISTORY_REMOVE_SELECTED_ITEMS},
      {"searchPrompt", IDS_HISTORY_SEARCH_PROMPT},
      {"searchResult", IDS_HISTORY_SEARCH_RESULT},
      {"searchResults", IDS_HISTORY_SEARCH_RESULTS},
      {"signInButton", IDS_HISTORY_SIGN_IN_BUTTON},
      {"signInPromo", IDS_HISTORY_SIGN_IN_PROMO},
      {"signInPromoDesc", IDS_HISTORY_SIGN_IN_PROMO_DESC},
      {"title", IDS_HISTORY_TITLE},
  };
  AddLocalizedStringsBulk(source, kStrings);

  source->AddString(
      "sidebarFooter",
      l10n_util::GetStringFUTF16(
          IDS_HISTORY_OTHER_FORMS_OF_HISTORY,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_CLEAR_DATA_MYACTIVITY_URL_IN_HISTORY)));

  PrefService* prefs = profile->GetPrefs();
  bool allow_deleting_history =
      prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory);
  source->AddBoolean("allowDeletingHistory", allow_deleting_history);

  source->AddBoolean(kShowMenuPromoKey, !MenuPromoShown(profile));
  source->AddBoolean("isGuestSession", profile->IsGuestSession());

  source->AddBoolean(kIsUserSignedInKey, IsUserSignedIn(profile));

#if BUILDFLAG(OPTIMIZE_WEBUI)
  source->AddResourcePath("app.html", IDR_HISTORY_APP_VULCANIZED_HTML);
  source->AddResourcePath("app.crisper.js", IDR_HISTORY_APP_CRISPER_JS);
  source->AddResourcePath("lazy_load.html",
                          IDR_HISTORY_LAZY_LOAD_VULCANIZED_HTML);
  source->AddResourcePath("lazy_load.crisper.js",
                          IDR_HISTORY_LAZY_LOAD_CRISPER_JS);
  source->AddResourcePath("constants.html", IDR_HISTORY_CONSTANTS_HTML);
  source->AddResourcePath("constants.js", IDR_HISTORY_CONSTANTS_JS);
  source->AddResourcePath("history.js", IDR_HISTORY_HISTORY_JS);
  source->AddResourcePath("history.js", IDR_HISTORY_HISTORY_JS);
  source->AddResourcePath("images/sign_in_promo.svg",
                          IDR_HISTORY_IMAGES_SIGN_IN_PROMO_SVG);
  source->AddResourcePath("images/sign_in_promo_dark.svg",
                          IDR_HISTORY_IMAGES_SIGN_IN_PROMO_DARK_SVG);
  source->AddResourcePath("strings.html", IDR_HISTORY_STRINGS_HTML);
#else
  // Add all history resources.
  for (size_t i = 0; i < kHistoryResourcesSize; ++i) {
    source->AddResourcePath(kHistoryResources[i].name,
                            kHistoryResources[i].value);
  }
#endif
  source->SetDefaultResource(IDR_HISTORY_HISTORY_HTML);
  source->UseStringsJs();

  return source;
}

}  // namespace

HistoryUI::HistoryUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* data_source = CreateHistoryUIHTMLSource(profile);
  ManagedUIHandler::Initialize(web_ui, data_source);
  content::WebUIDataSource::Add(profile, data_source);

  web_ui->AddMessageHandler(std::make_unique<webui::NavigationHandler>());
  web_ui->AddMessageHandler(std::make_unique<BrowsingHistoryHandler>());
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  web_ui->AddMessageHandler(
      std::make_unique<browser_sync::ForeignSessionHandler>());
  web_ui->AddMessageHandler(std::make_unique<HistoryLoginHandler>(
      base::Bind(&HistoryUI::UpdateDataSource, base::Unretained(this))));

  web_ui->RegisterMessageCallback(
      "menuPromoShown", base::BindRepeating(&HistoryUI::HandleMenuPromoShown,
                                            base::Unretained(this)));
}

HistoryUI::~HistoryUI() {}

void HistoryUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kHistoryMenuPromoShown, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void HistoryUI::UpdateDataSource() {
  CHECK(web_ui());

  Profile* profile = Profile::FromWebUI(web_ui());

  std::unique_ptr<base::DictionaryValue> update(new base::DictionaryValue);
  update->SetBoolean(kIsUserSignedInKey, IsUserSignedIn(profile));
  update->SetBoolean(kShowMenuPromoKey, !MenuPromoShown(profile));

  content::WebUIDataSource::Update(profile, chrome::kChromeUIHistoryHost,
                                   std::move(update));
}

void HistoryUI::HandleMenuPromoShown(const base::ListValue* args) {
  Profile::FromWebUI(web_ui())->GetPrefs()->SetBoolean(
      prefs::kHistoryMenuPromoShown, true);
  UpdateDataSource();
}
