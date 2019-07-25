// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy.h"

#include <utility>

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/app_service/public/mojom/constants.mojom.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connector.h"

namespace apps {

AppServiceProxy::InnerIconLoader::InnerIconLoader(AppServiceProxy* host)
    : host_(host), overriding_icon_loader_for_testing_(nullptr) {}

apps::mojom::IconKeyPtr AppServiceProxy::InnerIconLoader::GetIconKey(
    const std::string& app_id) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->GetIconKey(app_id);
  }

  apps::mojom::IconKeyPtr icon_key;
  if (host_->app_service_.is_bound()) {
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

  if (host_->app_service_.is_bound() && icon_key) {
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

// static
AppServiceProxy* AppServiceProxy::CreateForTesting(
    Profile* profile,
    service_manager::Connector* connector) {
  return new AppServiceProxy(profile, connector);
}

AppServiceProxy::AppServiceProxy(Profile* profile)
    : AppServiceProxy(profile, nullptr) {}

AppServiceProxy::AppServiceProxy(Profile* profile,
                                 service_manager::Connector* connector)
    : inner_icon_loader_(this),
      icon_coalescer_(&inner_icon_loader_),
      outer_icon_loader_(&icon_coalescer_,
                         apps::IconCache::GarbageCollectionPolicy::kEager) {
  if (!profile) {
    return;
  }
  if (!connector) {
    connector = content::BrowserContext::GetConnectorFor(profile);
    if (!connector) {
      return;
    }
  }
  connector->BindInterface(apps::mojom::kServiceName,
                           mojo::MakeRequest(&app_service_));

  if (app_service_.is_bound()) {
    // The AppServiceProxy is a subscriber: something that wants to be able
    // to list all known apps.
    apps::mojom::SubscriberPtr subscriber;
    bindings_.AddBinding(this, mojo::MakeRequest(&subscriber));
    app_service_->RegisterSubscriber(std::move(subscriber), nullptr);

#if defined(OS_CHROMEOS)
    // The AppServiceProxy is also a publisher, of a variety of app types.
    // That responsibility isn't intrinsically part of the AppServiceProxy,
    // but doing that here, for each such app type, is as good a place as any.
    built_in_chrome_os_apps_.Initialize(app_service_, profile);
    crostini_apps_.Initialize(app_service_, profile);
    extension_apps_.Initialize(app_service_, profile,
                               apps::mojom::AppType::kExtension);
    extension_web_apps_.Initialize(app_service_, profile,
                                   apps::mojom::AppType::kWeb);
#endif  // OS_CHROMEOS
  }
}

AppServiceProxy::~AppServiceProxy() = default;

apps::mojom::AppServicePtr& AppServiceProxy::AppService() {
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
  if (app_service_.is_bound()) {
    cache_.ForOneApp(app_id, [this, event_flags, launch_source,
                              display_id](const apps::AppUpdate& update) {
      app_service_->Launch(update.AppType(), update.AppId(), event_flags,
                           launch_source, display_id);
    });
  }
}

void AppServiceProxy::SetPermission(const std::string& app_id,
                                    apps::mojom::PermissionPtr permission) {
  if (app_service_.is_bound()) {
    cache_.ForOneApp(
        app_id, [this, &permission](const apps::AppUpdate& update) {
          app_service_->SetPermission(update.AppType(), update.AppId(),
                                      std::move(permission));
        });
  }
}

void AppServiceProxy::Uninstall(const std::string& app_id) {
  if (app_service_.is_bound()) {
    cache_.ForOneApp(app_id, [this](const apps::AppUpdate& update) {
      app_service_->Uninstall(update.AppType(), update.AppId());
    });
  }
}

void AppServiceProxy::OpenNativeSettings(const std::string& app_id) {
  if (app_service_.is_bound()) {
    cache_.ForOneApp(app_id, [this](const apps::AppUpdate& update) {
      app_service_->OpenNativeSettings(update.AppType(), update.AppId());
    });
  }
}

void AppServiceProxy::FlushMojoCallsForTesting() {
  bindings_.FlushForTesting();
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
  if (app_service_.is_bound()) {
    crostini_apps_.ReInitializeForTesting(app_service_, profile);
  }
#endif
}

void AppServiceProxy::Shutdown() {
#if defined(OS_CHROMEOS)
  if (app_service_.is_bound()) {
    extension_apps_.Shutdown();
    extension_web_apps_.Shutdown();
  }
#endif  // OS_CHROMEOS
}

void AppServiceProxy::OnApps(std::vector<apps::mojom::AppPtr> deltas) {
  cache_.OnApps(std::move(deltas));
}

void AppServiceProxy::Clone(apps::mojom::SubscriberRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace apps
