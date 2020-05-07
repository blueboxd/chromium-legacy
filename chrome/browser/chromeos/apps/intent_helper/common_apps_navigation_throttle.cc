// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/intent_helper/common_apps_navigation_throttle.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/types/display_constants.h"

namespace {

apps::PickerEntryType GetPickerEntryType(apps::mojom::AppType app_type) {
  apps::PickerEntryType picker_entry_type = apps::PickerEntryType::kUnknown;
  switch (app_type) {
    case apps::mojom::AppType::kUnknown:
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kCrostini:
    case apps::mojom::AppType::kPluginVm:
    case apps::mojom::AppType::kExtension:
      break;
    case apps::mojom::AppType::kArc:
      picker_entry_type = apps::PickerEntryType::kArc;
      break;
    case apps::mojom::AppType::kWeb:
      picker_entry_type = apps::PickerEntryType::kWeb;
      break;
    case apps::mojom::AppType::kMacNative:
      picker_entry_type = apps::PickerEntryType::kMacNative;
      break;
  }
  return picker_entry_type;
}

}  // namespace

namespace apps {

// static
std::unique_ptr<apps::AppsNavigationThrottle>
CommonAppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return nullptr;

  content::WebContents* web_contents = handle->GetWebContents();

  if (!apps::AppsNavigationThrottle::CanCreate(web_contents))
    return nullptr;

  return std::make_unique<CommonAppsNavigationThrottle>(handle);
}

// static
void CommonAppsNavigationThrottle::ShowIntentPickerBubble(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  std::vector<apps::IntentPickerAppInfo> apps_for_picker =
      FindAllAppsForUrl(web_contents, url, {});

  IntentPickerTabHelper::LoadAppIcons(
      web_contents, std::move(apps_for_picker),
      base::BindOnce(&OnAppIconsLoaded, web_contents, ui_auto_display_service,
                     url));
}

// static
void CommonAppsNavigationThrottle::OnIntentPickerClosed(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    const std::string& launch_name,
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason close_reason,
    bool should_persist) {
  const bool should_launch_app =
      close_reason == apps::IntentPickerCloseReason::OPEN_APP;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  if (!proxy)
    return;

  if (should_persist)
    proxy->AddPreferredApp(launch_name, url);

  if (should_launch_app) {
    // TODO(crbug.com/853604): Distinguish the source from link and omnibox.
    apps::mojom::LaunchSource launch_source =
        apps::mojom::LaunchSource::kFromLink;
    proxy->LaunchAppWithUrl(
        launch_name,
        GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                      WindowOpenDisposition::NEW_WINDOW,
                      /*prefer_container=*/true),
        url, launch_source, display::kDefaultDisplayId);
    CloseOrGoBack(web_contents);
  }
}

// static
void CommonAppsNavigationThrottle::OnAppIconsLoaded(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    std::vector<apps::IntentPickerAppInfo> apps) {
  apps::AppsNavigationThrottle::ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps),
      /*show_stay_in_chrome=*/true,
      /*show_remember_selection=*/true,
      base::BindOnce(&OnIntentPickerClosed, web_contents,
                     ui_auto_display_service, url));
}

CommonAppsNavigationThrottle::CommonAppsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : apps::AppsNavigationThrottle(navigation_handle) {}

CommonAppsNavigationThrottle::~CommonAppsNavigationThrottle() = default;

std::vector<apps::IntentPickerAppInfo>
CommonAppsNavigationThrottle::FindAppsForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<apps::IntentPickerAppInfo> apps) {
  return FindAllAppsForUrl(web_contents, url, std::move(apps));
}

// static
std::vector<apps::IntentPickerAppInfo>
CommonAppsNavigationThrottle::FindAllAppsForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<apps::IntentPickerAppInfo> apps) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  if (!proxy)
    return apps;

  std::vector<std::string> app_ids = proxy->GetAppIdsForUrl(url);

  auto preferred_app_id = proxy->PreferredApps().FindPreferredAppForUrl(url);

  for (const std::string& app_id : app_ids) {
    proxy->AppRegistryCache().ForOneApp(
        app_id, [&preferred_app_id, &apps](const apps::AppUpdate& update) {
          // TODO(crbug.com/853604): Automatically launch the app. At the moment
          // just mark the app as preferred to minimize the change to
          // AppsNavigationThrottle.
          std::string display_name = update.Name();
          if (update.AppId() == preferred_app_id)
            display_name = update.Name() + " (preferred)";
          apps.emplace(apps.begin(), GetPickerEntryType(update.AppType()),
                       gfx::Image(), update.AppId(), display_name);
        });
  }
  return apps;
}

apps::AppsNavigationThrottle::PickerShowState
CommonAppsNavigationThrottle::GetPickerShowState(
    const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  return PickerShowState::kOmnibox;
}

IntentPickerResponse CommonAppsNavigationThrottle::GetOnPickerClosedCallback(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  return base::BindOnce(&OnIntentPickerClosed, web_contents,
                        ui_auto_display_service, url);
}

bool CommonAppsNavigationThrottle::ShouldDeferNavigation(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();

  const GURL& url = handle->GetURL();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  if (!proxy)
    return false;

  std::vector<std::string> app_ids = proxy->GetAppIdsForUrl(url);

  if (app_ids.empty())
    return false;

  if (navigate_from_link()) {
    auto preferred_app_id = proxy->PreferredApps().FindPreferredAppForUrl(url);

    if (preferred_app_id.has_value() &&
        base::Contains(app_ids, preferred_app_id.value())) {
      // Only automatically launch PWA if the flag is on.
      auto app_type =
          proxy->AppRegistryCache().GetAppType(preferred_app_id.value());
      if (app_type == apps::mojom::AppType::kArc ||
          (app_type == apps::mojom::AppType::kWeb &&
           base::FeatureList::IsEnabled(
               features::kIntentPickerPWAPersistence))) {
        auto launch_source = apps::mojom::LaunchSource::kFromLink;
        proxy->LaunchAppWithUrl(
            preferred_app_id.value(),
            GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true),
            url, launch_source, display::kDefaultDisplayId);
        CloseOrGoBack(web_contents);
        return true;
      }
    }
  }

  std::vector<apps::IntentPickerAppInfo> apps_for_picker =
      FindAllAppsForUrl(web_contents, url, {});

  if (apps_for_picker.empty())
    return false;

  IntentPickerTabHelper::LoadAppIcons(
      web_contents, std::move(apps_for_picker),
      base::BindOnce(
          &CommonAppsNavigationThrottle::OnDeferredNavigationProcessed,
          weak_factory_.GetWeakPtr()));
  return true;
}

void CommonAppsNavigationThrottle::OnDeferredNavigationProcessed(
    std::vector<apps::IntentPickerAppInfo> apps) {
  content::NavigationHandle* handle = navigation_handle();
  content::WebContents* web_contents = handle->GetWebContents();
  const GURL& url = handle->GetURL();

  ShowIntentPickerForApps(web_contents, ui_auto_display_service_, url,
                          std::move(apps),
                          base::BindOnce(&OnIntentPickerClosed, web_contents,
                                         ui_auto_display_service_, url));

  // We are about to resume the navigation, which may destroy this object.
  Resume();
}

}  // namespace apps
