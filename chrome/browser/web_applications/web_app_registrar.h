// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/abstract_web_app_sync_bridge.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

class WebApp;
class WebAppRegistryUpdate;

// A registry. This is a read-only container, which owns WebApp objects.
class WebAppRegistrar : public AppRegistrar {
 public:
  explicit WebAppRegistrar(Profile* profile,
                           AbstractWebAppSyncBridge* sync_bridge);
  ~WebAppRegistrar() override;

  void RegisterApp(std::unique_ptr<WebApp> web_app);
  std::unique_ptr<WebApp> UnregisterApp(const AppId& app_id);
  void UnregisterAll();

  bool is_empty() const { return registry_.empty(); }

  const WebApp* GetAppById(const AppId& app_id) const;

  using CommitCallback = base::OnceCallback<void(bool success)>;

  // This is the writable API for the registry. Any updates should be written to
  // LevelDb. There can be only 1 update at a time.
  std::unique_ptr<WebAppRegistryUpdate> BeginUpdate();
  void CommitUpdate(std::unique_ptr<WebAppRegistryUpdate> update,
                    CommitCallback callback);

  // AppRegistrar:
  void Init(base::OnceClosure callback) override;
  WebAppRegistrar* AsWebAppRegistrar() override;
  bool IsInstalled(const AppId& app_id) const override;
  bool IsLocallyInstalled(const AppId& app_id) const override;
  bool WasExternalAppUninstalledByUser(const AppId& app_id) const override;
  bool WasInstalledByUser(const AppId& app_id) const override;
  base::Optional<AppId> FindAppWithUrlInScope(const GURL& url) const override;
  int CountUserInstalledApps() const override;
  std::string GetAppShortName(const AppId& app_id) const override;
  std::string GetAppDescription(const AppId& app_id) const override;
  base::Optional<SkColor> GetAppThemeColor(const AppId& app_id) const override;
  const GURL& GetAppLaunchURL(const AppId& app_id) const override;
  base::Optional<GURL> GetAppScope(const AppId& app_id) const override;
  LaunchContainer GetAppLaunchContainer(const AppId& app_id) const override;
  void SetAppLaunchContainer(const AppId& app_id,
                             LaunchContainer launch_container) override;
  std::vector<AppId> GetAppIds() const override;

  // Only range-based |for| loop supported. Don't use AppSet directly.
  // Doesn't support registration and unregistration of WebApp while iterating.
  class AppSet {
   public:
    // An iterator class that can be used to access the list of apps.
    template <typename WebAppType>
    class Iter {
     public:
      using InternalIter = Registry::const_iterator;

      explicit Iter(InternalIter&& internal_iter)
          : internal_iter_(std::move(internal_iter)) {}
      Iter(Iter&&) = default;
      ~Iter() = default;

      void operator++() { ++internal_iter_; }
      WebAppType& operator*() const { return *internal_iter_->second.get(); }
      bool operator!=(const Iter& iter) const {
        return internal_iter_ != iter.internal_iter_;
      }

     private:
      InternalIter internal_iter_;
      DISALLOW_COPY_AND_ASSIGN(Iter);
    };

    explicit AppSet(const WebAppRegistrar* registrar);
    AppSet(AppSet&&) = default;
    ~AppSet();

    using iterator = Iter<WebApp>;
    using const_iterator = Iter<const WebApp>;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

   private:
    const WebAppRegistrar* registrar_;
#if DCHECK_IS_ON()
    const size_t mutations_count_;
#endif
    DISALLOW_COPY_AND_ASSIGN(AppSet);
  };

  const AppSet AllApps() const;

  const Registry& registry_for_testing() const { return registry_; }

 private:
  friend class WebAppRegistryUpdate;
  FRIEND_TEST_ALL_PREFIXES(WebAppRegistrarTest, AllAppsMutable);

  WebApp* GetAppByIdMutable(const AppId& app_id);
  AppSet AllAppsMutable();

  void OnDatabaseOpened(base::OnceClosure callback, Registry registry);
  void OnDataWritten(CommitCallback callback, bool success);

  void CountMutation();

  Registry registry_;

  // An abstract sync bridge to handle all local changes and persistence.
  AbstractWebAppSyncBridge* sync_bridge_;

  bool is_in_update_ = false;

#if DCHECK_IS_ON()
  size_t mutations_count_ = 0;
#endif

  base::WeakPtrFactory<WebAppRegistrar> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppRegistrar);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
