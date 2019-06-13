// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_IMPL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/services/app_service/public/cpp/app_service_proxy.h"
#include "chrome/services/app_service/public/cpp/icon_cache.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding_set.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/apps/app_service/built_in_chromeos_apps.h"
#include "chrome/browser/apps/app_service/crostini_apps.h"
#include "chrome/browser/apps/app_service/extension_apps.h"
#endif  // OS_CHROMEOS

class Profile;

namespace apps {

// Singleton (per Profile) proxy and cache of an App Service's apps.
//
// Singleton-ness means that //chrome/browser code (e.g UI code) can find *the*
// proxy for a given Profile, and therefore share its caches.
//
// See chrome/services/app_service/README.md.
class AppServiceProxyImpl : public KeyedService,
                            public apps::AppServiceProxy,
                            public apps::mojom::Subscriber {
 public:
  // This method returns an AppServiceProxyImpl, not just an AppServiceProxy, so
  // that callers (which are presumably in test code) can then call other
  // XxxForTesting methods.
  //
  // For regular (non-test) code, use AppServiceProxyFactory::GetForProfile
  // instead.
  static AppServiceProxyImpl* GetImplForTesting(Profile* profile);

  explicit AppServiceProxyImpl(Profile* profile);

  ~AppServiceProxyImpl() override;

  // apps::AppServiceProxy (including apps::IconLoader) overrides.
  apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override;
  std::unique_ptr<IconLoader::Releaser> LoadIconFromIconKey(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::IconKeyPtr icon_key,
      apps::mojom::IconCompression icon_compression,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::mojom::Publisher::LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void Uninstall(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;

  void FlushMojoCallsForTesting();
  apps::IconLoader* OverrideInnerIconLoaderForTesting(
      apps::IconLoader* icon_loader);
  void ReInitializeCrostiniForTesting(Profile* profile);

 private:
  // An adapter, presenting an IconLoader interface based on the underlying
  // Mojo service (or on a fake implementation for testing).
  //
  // Conceptually, the ASP (the AppServiceProxyImpl) is itself such an adapter:
  // UI clients call the IconLoader::LoadIconFromIconKey method (which the ASP
  // implements) and the ASP translates (i.e. adapts) these to Mojo calls (or
  // C++ calls to the Fake). This diagram shows control flow going left to
  // right (with "=c=>" and "=m=>" denoting C++ and Mojo calls), and the
  // responses (callbacks) then run right to left in LIFO order:
  //
  //   UI =c=> ASP =+=m=> MojoService
  //                |       or
  //                +=c=> Fake
  //
  // It is more complicated in practice, as we want to insert IconLoader
  // decorators (as in the classic "decorator" or "wrapper" design pattern) to
  // provide optimizations like proxy-wide icon caching and IPC coalescing
  // (de-duplication). Nonetheless, from a UI client's point of view, we would
  // still like to present a simple API: that the ASP implements the IconLoader
  // interface. We therefore split the "ASP" component into multiple
  // sub-components. Once again, control flow runs from left to right, and
  // inside the ASP, outer layers (wrappers) call into inner layers (wrappees):
  //
  //           +------------------ ASP ------------------+
  //           |                                         |
  //   UI =c=> | Outer =c=> MoreDecorators... =c=> Inner | =+=m=> MojoService
  //           |                                         |  |       or
  //           +-----------------------------------------+  +=c=> Fake
  //
  // The inner_icon_loader_ field (of type InnerIconLoader) is the "Inner"
  // component: the one that ultimately talks to the Mojo service.
  //
  // The outer_icon_loader_ field (of type IconCache) is the "Outer" component:
  // the entry point for calls into the AppServiceProxyImpl.
  //
  // Note that even if the ASP provides some icon caching, upstream UI clients
  // may want to introduce further icon caching. See the commentary where
  // IconCache::GarbageCollectionPolicy is defined.
  //
  // IPC coalescing would be one of the "MoreDecorators".
  class InnerIconLoader : public apps::IconLoader {
   public:
    explicit InnerIconLoader(AppServiceProxyImpl* host);

    // apps::IconLoader overrides.
    apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override;
    std::unique_ptr<IconLoader::Releaser> LoadIconFromIconKey(
        apps::mojom::AppType app_type,
        const std::string& app_id,
        apps::mojom::IconKeyPtr icon_key,
        apps::mojom::IconCompression icon_compression,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon,
        apps::mojom::Publisher::LoadIconCallback callback) override;

    // |host_| owns |this|, as the InnerIconLoader is an AppServiceProxyImpl
    // field.
    AppServiceProxyImpl* host_;

    apps::IconLoader* overriding_icon_loader_for_testing_;
  };

  // KeyedService overrides.
  void Shutdown() override;

  // apps::mojom::Subscriber overrides.
  void OnApps(std::vector<apps::mojom::AppPtr> deltas) override;
  void Clone(apps::mojom::SubscriberRequest request) override;

  mojo::BindingSet<apps::mojom::Subscriber> bindings_;

  // The LoadIconFromIconKey implementation sends a chained series of requests
  // through each icon loader, starting from the outer and working back to the
  // inner. Fields are listed from inner to outer, the opposite of call order,
  // as each one depends on the previous one, and in the constructor,
  // initialization happens in field order.
  InnerIconLoader inner_icon_loader_;
  IconCache outer_icon_loader_;

#if defined(OS_CHROMEOS)
  BuiltInChromeOsApps built_in_chrome_os_apps_;
  CrostiniApps crostini_apps_;
  ExtensionApps extension_apps_;
  ExtensionApps extension_web_apps_;
#endif  // OS_CHROMEOS

  DISALLOW_COPY_AND_ASSIGN(AppServiceProxyImpl);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_IMPL_H_
