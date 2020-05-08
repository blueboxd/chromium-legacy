// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/os_settings_provider.h"

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/web_applications/default_web_app_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler_factory.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace app_list {
namespace {

constexpr char kOsSettingsResultPrefix[] = "os-settings://";

// Various error states of the OsSettingsProvider. kOk is currently not emitted,
// but may be used in future. These values persist to logs. Entries should not
// be renumbered and numeric values should never be reused.
enum class Error {
  kOk = 0,
  kAppServiceUnavailable = 1,
  kNoSettingsIcon = 2,
  kSearchHandlerUnavailable = 3,
  kMaxValue = kSearchHandlerUnavailable
};

void LogError(Error error) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.OsSettingsProvider.Error", error);
}

}  // namespace

OsSettingsResult::OsSettingsResult(
    Profile* profile,
    const chromeos::settings::mojom::SearchResultPtr& result,
    const gfx::ImageSkia& icon)
    : profile_(profile), url_path_(result->url_path_with_parameters) {
  // TODO(crbug.com/1068851): Results need a useful relevance score and details
  // text. Once this is available in the SearchResultPtr, set the metadata here.
  set_id(kOsSettingsResultPrefix + url_path_);
  set_relevance(8.0f);
  SetTitle(result->result_text);
  SetResultType(ResultType::kOsSettings);
  SetDisplayType(DisplayType::kList);
  SetIcon(icon);
}

OsSettingsResult::~OsSettingsResult() = default;

void OsSettingsResult::Open(int event_flags) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile_,
                                                               url_path_);
}

ash::SearchResultType OsSettingsResult::GetSearchResultType() const {
  return ash::OS_SETTINGS;
}

OsSettingsProvider::OsSettingsProvider(Profile* profile)
    : profile_(profile),
      search_handler_(
          chromeos::settings::SearchHandlerFactory::GetForProfile(profile)) {
  DCHECK(profile_);

  // |search_handler_| can be nullptr in the case that the new OS settings
  // search chrome flag is disabled. If it is, we should effectively disable the
  // search provider.
  if (!search_handler_) {
    LogError(Error::kSearchHandlerUnavailable);
    return;
  }

  app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile_);
  if (app_service_proxy_) {
    Observe(&app_service_proxy_->AppRegistryCache());
    app_service_proxy_->LoadIcon(
        apps::mojom::AppType::kWeb,
        chromeos::default_web_apps::kOsSettingsAppId,
        apps::mojom::IconCompression::kUncompressed,
        ash::AppListConfig::instance().search_list_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        base::BindOnce(&OsSettingsProvider::OnLoadIcon,
                       weak_factory_.GetWeakPtr()));
  } else {
    LogError(Error::kAppServiceUnavailable);
  }
}

OsSettingsProvider::~OsSettingsProvider() = default;

void OsSettingsProvider::Start(const base::string16& query) {
  if (!search_handler_)
    return;

  ClearResultsSilently();

  // This provider does not handle zero-state.
  if (query.empty())
    return;

  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
  // TODO(crbug.com/1068851): There are currently only a handful of settings
  // returned from the backend. Once the search service has finished integration
  // into settings, verify we see all results here, and that opening works
  // correctly for the new URLs.
  search_handler_->Search(query,
                          base::BindOnce(&OsSettingsProvider::OnSearchReturned,
                                         weak_factory_.GetWeakPtr()));
}

void OsSettingsProvider::OnSearchReturned(
    std::vector<chromeos::settings::mojom::SearchResultPtr> results) {
  SearchProvider::Results search_results;
  for (const auto& result : results) {
    search_results.emplace_back(
        std::make_unique<OsSettingsResult>(profile_, result, icon_));
  }

  if (icon_.isNull())
    LogError(Error::kNoSettingsIcon);
  SwapResults(&search_results);
}

void OsSettingsProvider::OnAppUpdate(const apps::AppUpdate& update) {
  // Watch the app service for updates. On an update that marks the OS settings
  // app as ready, retrieve the icon for the app to use for search results.
  if (app_service_proxy_ &&
      update.AppId() == chromeos::default_web_apps::kOsSettingsAppId &&
      update.ReadinessChanged() &&
      update.Readiness() == apps::mojom::Readiness::kReady) {
    app_service_proxy_->LoadIcon(
        apps::mojom::AppType::kWeb,
        chromeos::default_web_apps::kOsSettingsAppId,
        apps::mojom::IconCompression::kUncompressed,
        ash::AppListConfig::instance().search_list_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        base::BindOnce(&OsSettingsProvider::OnLoadIcon,
                       weak_factory_.GetWeakPtr()));
  }
}

void OsSettingsProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void OsSettingsProvider::OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression ==
      apps::mojom::IconCompression::kUncompressed) {
    icon_ = icon_value->uncompressed;
  }
}

}  // namespace app_list
