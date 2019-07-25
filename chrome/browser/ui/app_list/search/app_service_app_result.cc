// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_service_app_result.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_service_app_item.h"
#include "chrome/browser/ui/app_list/search/internal_app_result.h"
#include "extensions/common/extension.h"

namespace app_list {

AppServiceAppResult::AppServiceAppResult(Profile* profile,
                                         const std::string& app_id,
                                         AppListControllerDelegate* controller,
                                         bool is_recommendation,
                                         apps::IconLoader* icon_loader)
    : AppResult(profile, app_id, controller, is_recommendation),
      icon_loader_(icon_loader),
      app_type_(apps::mojom::AppType::kUnknown),
      is_platform_app_(false),
      show_in_launcher_(false),
      weak_ptr_factory_(this) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  if (proxy) {
    proxy->AppRegistryCache().ForOneApp(
        app_id, [this](const apps::AppUpdate& update) {
          app_type_ = update.AppType();
          is_platform_app_ =
              update.IsPlatformApp() == apps::mojom::OptionalBool::kTrue;
          show_in_launcher_ =
              update.ShowInLauncher() == apps::mojom::OptionalBool::kTrue;
        });

    constexpr bool allow_placeholder_icon = true;
    CallLoadIcon(false, allow_placeholder_icon);
    if (display_type() == ash::SearchResultDisplayType::kRecommendation) {
      CallLoadIcon(true, allow_placeholder_icon);
    }
  }

  switch (app_type_) {
    case apps::mojom::AppType::kBuiltIn:
      set_id(app_id);
      // TODO(crbug.com/826982): Is this SetResultType call necessary?? Does
      // anyone care about the kInternalApp vs kInstalledApp distinction?
      SetResultType(ResultType::kInternalApp);
      // TODO(crbug.com/826982): Move this from the App Service caller to
      // callee, closer to where other histograms are updated in
      // BuiltInChromeOsApps::Launch??
      InternalAppResult::RecordShowHistogram(app_id);
      break;
    case apps::mojom::AppType::kExtension:
      // TODO(crbug.com/826982): why do we pass the URL and not the app_id??
      // Can we replace this by the simpler "set_id(app_id)", and therefore
      // pull that out of the switch?
      set_id(extensions::Extension::GetBaseURLFromExtensionId(app_id).spec());
      break;
    default:
      set_id(app_id);
      break;
  }
}

AppServiceAppResult::~AppServiceAppResult() = default;

void AppServiceAppResult::Open(int event_flags) {
  Launch(event_flags,
         (display_type() == ash::SearchResultDisplayType::kRecommendation)
             ? apps::mojom::LaunchSource::kFromAppListRecommendation
             : apps::mojom::LaunchSource::kFromAppListQuery);
}

void AppServiceAppResult::GetContextMenuModel(GetMenuModelCallback callback) {
  // TODO(crbug.com/826982): drop the (app_type_ == etc), and check
  // show_in_launcher_ for all app types?
  if ((app_type_ == apps::mojom::AppType::kBuiltIn) && !show_in_launcher_) {
    std::move(callback).Run(nullptr);
    return;
  }

  context_menu_ = AppServiceAppItem::MakeAppContextMenu(
      app_type_, this, profile(), app_id(), controller(), is_platform_app_);
  context_menu_->GetMenuModel(std::move(callback));
}

SearchResultType AppServiceAppResult::GetSearchResultType() const {
  switch (app_type_) {
    case apps::mojom::AppType::kArc:
      return PLAY_STORE_APP;
    case apps::mojom::AppType::kBuiltIn:
      return INTERNAL_APP;
    case apps::mojom::AppType::kCrostini:
      return CROSTINI_APP;
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kWeb:
      return EXTENSION_APP;
    default:
      NOTREACHED();
      return SEARCH_RESULT_TYPE_BOUNDARY;
  }
}

AppContextMenu* AppServiceAppResult::GetAppContextMenu() {
  return context_menu_.get();
}

void AppServiceAppResult::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags, apps::mojom::LaunchSource::kFromAppListQueryContextMenu);
}

void AppServiceAppResult::Launch(int event_flags,
                                 apps::mojom::LaunchSource launch_source) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  if (proxy) {
    proxy->Launch(app_id(), event_flags, launch_source,
                  controller()->GetAppListDisplayId());
  }
}

void AppServiceAppResult::CallLoadIcon(bool chip, bool allow_placeholder_icon) {
  if (icon_loader_) {
    // If |icon_loader_releaser_| is non-null, assigning to it will signal to
    // |icon_loader_| that the previous icon is no longer being used, as a hint
    // that it could be flushed from any caches.
    icon_loader_releaser_ = icon_loader_->LoadIcon(
        app_type_, app_id(), apps::mojom::IconCompression::kUncompressed,
        chip ? AppListConfig::instance().suggestion_chip_icon_dimension()
             : AppListConfig::instance().GetPreferredIconDimension(
                   display_type()),
        allow_placeholder_icon,
        base::BindOnce(&AppServiceAppResult::OnLoadIcon,
                       weak_ptr_factory_.GetWeakPtr(), chip));
  }
}

void AppServiceAppResult::OnLoadIcon(bool chip,
                                     apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression !=
      apps::mojom::IconCompression::kUncompressed) {
    return;
  }

  if (chip) {
    SetChipIcon(icon_value->uncompressed);
  } else {
    SetIcon(icon_value->uncompressed);
  }

  if (icon_value->is_placeholder_icon) {
    constexpr bool allow_placeholder_icon = false;
    CallLoadIcon(chip, allow_placeholder_icon);
  }
}

}  // namespace app_list
