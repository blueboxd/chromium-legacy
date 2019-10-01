// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/app_service/app_service_impl.h"
#include "chrome/services/app_service/public/cpp/intent_util.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/url_data_source.h"

namespace apps {

AppServiceProxy::InnerIconLoader::InnerIconLoader(AppServiceProxy* host)
    : host_(host), overriding_icon_loader_for_testing_(nullptr) {}

apps::mojom::IconKeyPtr AppServiceProxy::InnerIconLoader::GetIconKey(
    const std::string& app_id) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->GetIconKey(app_id);
  }

  apps::mojom::IconKeyPtr icon_key;
  if (host_->app_service_.is_connected()) {
    host_->cache_.ForOneApp(app_id, [&icon_key](const apps::AppUpdate& update) {
      icon_key = update.IconKey();
    });
  }
  return icon_key;
}

std::unique_ptr<IconLoader::Releaser>
AppServiceProxy::InnerIconLoader::LoadIconFromIconKey(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->LoadIconFromIconKey(
        app_type, app_id, std::move(icon_key), icon_compression,
        size_hint_in_dip, allow_placeholder_icon, std::move(callback));
  }

  if (host_->app_service_.is_connected() && icon_key) {
    // TODO(crbug.com/826982): Mojo doesn't guarantee the order of messages,
    // so multiple calls to this method might not resolve their callbacks in
    // order. As per khmel@, "you may have race here, assume you publish change
    // for the app and app requested new icon. But new icon is not delivered
    // yet and you resolve old one instead. Now new icon arrives asynchronously
    // but you no longer notify the app or do?"
    host_->app_service_->LoadIcon(app_type, app_id, std::move(icon_key),
                                  icon_compression, size_hint_in_dip,
                                  allow_placeholder_icon, std::move(callback));
  } else {
    std::move(callback).Run(apps::mojom::IconValue::New());
  }
  return nullptr;
}

AppServiceProxy::AppServiceProxy(Profile* profile)
    : inner_icon_loader_(this),
      icon_coalescer_(&inner_icon_loader_),
      outer_icon_loader_(&icon_coalescer_,
                         apps::IconCache::GarbageCollectionPolicy::kEager) {
  Initialize(profile);
}

AppServiceProxy::~AppServiceProxy() = default;

void AppServiceProxy::ReInitializeForTesting(Profile* profile) {
  // Some test code creates a profile and profile-linked services, like the App
  // Service, before the profile is fully initialized. Such tests can call this
  // after full profile initialization to ensure the App Service implementation
  // has all of profile state it needs.
  app_service_.reset();
  Initialize(profile);
}

void AppServiceProxy::Initialize(Profile* profile) {
  if (!profile) {
    return;
  }

  // We only initialize the App Service for regular or guest profiles. Non-guest
  // off-the-record profiles do not get an instance.
  if (profile->IsOffTheRecord() && !profile->IsGuestSession()) {
    return;
  }

  app_service_impl_ = std::make_unique<apps::AppServiceImpl>();
  app_service_impl_->BindReceiver(app_service_.BindNewPipeAndPassReceiver());

  if (app_service_.is_connected()) {
    // The AppServiceProxy is a subscriber: something that wants to be able to
    // list all known apps.
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber;
    receivers_.Add(this, subscriber.InitWithNewPipeAndPassReceiver());
    app_service_->RegisterSubscriber(std::move(subscriber), nullptr);

#if defined(OS_CHROMEOS)
    // The AppServiceProxy is also a publisher, of a variety of app types. That
    // responsibility isn't intrinsically part of the AppServiceProxy, but doing
    // that here, for each such app type, is as good a place as any.
    built_in_chrome_os_apps_ =
        std::make_unique<BuiltInChromeOsApps>(app_service_, profile);
    crostini_apps_ = std::make_unique<CrostiniApps>(app_service_, profile);
    extension_apps_ = std::make_unique<ExtensionApps>(
        app_service_, profile, apps::mojom::AppType::kExtension);
    extension_web_apps_ = std::make_unique<ExtensionApps>(
        app_service_, profile, apps::mojom::AppType::kWeb);

    // Asynchronously add app icon source, so we don't do too much work in the
    // constructor.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AppServiceProxy::AddAppIconSource,
                                  weak_ptr_factory_.GetWeakPtr(), profile));
#endif  // OS_CHROMEOS
  }
}

mojo::Remote<apps::mojom::AppService>& AppServiceProxy::AppService() {
  return app_service_;
}

apps::AppRegistryCache& AppServiceProxy::AppRegistryCache() {
  return cache_;
}

apps::mojom::IconKeyPtr AppServiceProxy::GetIconKey(const std::string& app_id) {
  return outer_icon_loader_.GetIconKey(app_id);
}

std::unique_ptr<apps::IconLoader::Releaser>
AppServiceProxy::LoadIconFromIconKey(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  return outer_icon_loader_.LoadIconFromIconKey(
      app_type, app_id, std::move(icon_key), icon_compression, size_hint_in_dip,
      allow_placeholder_icon, std::move(callback));
}

void AppServiceProxy::Launch(const std::string& app_id,
                             int32_t event_flags,
                             apps::mojom::LaunchSource launch_source,
                             int64_t display_id) {
  if (app_service_.is_connected()) {
    cache_.ForOneApp(app_id, [this, event_flags, launch_source,
                              display_id](const apps::AppUpdate& update) {
      RecordAppLaunch(update.AppId(), launch_source);
      app_service_->Launch(update.AppType(), update.AppId(), event_flags,
                           launch_source, display_id);
    });
  }
}

void AppServiceProxy::SetPermission(const std::string& app_id,
                                    apps::mojom::PermissionPtr permission) {
  if (app_service_.is_connected()) {
    cache_.ForOneApp(
        app_id, [this, &permission](const apps::AppUpdate& update) {
          app_service_->SetPermission(update.AppType(), update.AppId(),
                                      std::move(permission));
        });
  }
}

void AppServiceProxy::Uninstall(const std::string& app_id) {
  if (app_service_.is_connected()) {
    cache_.ForOneApp(app_id, [this](const apps::AppUpdate& update) {
      app_service_->Uninstall(update.AppType(), update.AppId());
    });
  }
}

void AppServiceProxy::OpenNativeSettings(const std::string& app_id) {
  if (app_service_.is_connected()) {
    cache_.ForOneApp(app_id, [this](const apps::AppUpdate& update) {
      app_service_->OpenNativeSettings(update.AppType(), update.AppId());
    });
  }
}

void AppServiceProxy::FlushMojoCallsForTesting() {
  app_service_impl_->FlushMojoCallsForTesting();
#if defined(OS_CHROMEOS)
  built_in_chrome_os_apps_->FlushMojoCallsForTesting();
  crostini_apps_->FlushMojoCallsForTesting();
  extension_apps_->FlushMojoCallsForTesting();
  extension_web_apps_->FlushMojoCallsForTesting();
#endif
  receivers_.FlushForTesting();
}

apps::IconLoader* AppServiceProxy::OverrideInnerIconLoaderForTesting(
    apps::IconLoader* icon_loader) {
  apps::IconLoader* old =
      inner_icon_loader_.overriding_icon_loader_for_testing_;
  inner_icon_loader_.overriding_icon_loader_for_testing_ = icon_loader;
  return old;
}

void AppServiceProxy::ReInitializeCrostiniForTesting(Profile* profile) {
#if defined(OS_CHROMEOS)
  if (app_service_.is_connected()) {
    crostini_apps_->ReInitializeForTesting(app_service_, profile);
  }
#endif
}

std::vector<std::string> AppServiceProxy::GetAppIdsForUrl(const GURL& url) {
  return GetAppIdsForIntent(apps_util::CreateIntentFromUrl(url));
}

std::vector<std::string> AppServiceProxy::GetAppIdsForIntent(
    apps::mojom::IntentPtr intent) {
  std::vector<std::string> app_ids;
  if (app_service_.is_bound()) {
    cache_.ForEachApp([&app_ids, &intent](const apps::AppUpdate& update) {
      for (const auto& filter : update.IntentFilters()) {
        if (apps_util::IntentMatchesFilter(intent, filter)) {
          app_ids.push_back(update.AppId());
        }
      }
    });
  }
  return app_ids;
}

void AppServiceProxy::AddAppIconSource(Profile* profile) {
  // Make the chrome://app-icon/ resource available.
  content::URLDataSource::Add(profile,
                              std::make_unique<apps::AppIconSource>(profile));
}

void AppServiceProxy::Shutdown() {
#if defined(OS_CHROMEOS)
  if (app_service_.is_connected()) {
    extension_apps_->Shutdown();
    extension_web_apps_->Shutdown();
  }
#endif  // OS_CHROMEOS
}

void AppServiceProxy::OnApps(std::vector<apps::mojom::AppPtr> deltas) {
  cache_.OnApps(std::move(deltas));
}

void AppServiceProxy::Clone(
    mojo::PendingReceiver<apps::mojom::Subscriber> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace apps
