// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRAR_H_

#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

class GURL;
class Profile;

namespace web_app {

class AppRegistrarObserver;

class AppRegistrar {
 public:
  explicit AppRegistrar(Profile* profile);
  virtual ~AppRegistrar();

  virtual void Init(base::OnceClosure callback) = 0;

  // Returns true if the app with the specified |start_url| is currently fully
  // locally installed. The provided |start_url| must exactly match the launch
  // URL for the app; this method does not consult the app scope or match URLs
  // that fall within the scope.
  virtual bool IsInstalled(const GURL& start_url) const = 0;

  // Returns true if the app with |app_id| is currently installed.
  virtual bool IsInstalled(const AppId& app_id) const = 0;

  // Returns true if the app with |app_id| was previously uninstalled by the
  // user. For example, if a user uninstalls a default app ('default apps' are
  // considered external apps), then this will return true.
  virtual bool WasExternalAppUninstalledByUser(const AppId& app_id) const = 0;

  // Returns true if the app has a scope (some legacy apps don't). See
  // GetScopeUrlForApp() for more context about scope.
  virtual bool HasScopeUrl(const AppId& app_id) const = 0;

  // Scope is the navigation scope of this WebApp. This means all URLs that the
  // app considers part of itself.
  //
  // Some legacy apps don't have a scope and returning an empty GURL for them
  // could lead to incorrecly thinking some URLs are in-scope of the app. To
  // avoid this, this method CHECKs if the app doesn't have a scope. Callers can
  // use HasScopeUrl() to know if the app has a scope before calling this
  // method.
  virtual GURL GetScopeUrlForApp(const AppId& app_id) const = 0;

  // Returns the app id for which the |url| is in scope of, empty if none.
  virtual AppId FindAppIdForUrl(const GURL& url) const = 0;

  // Count a number of all apps which are installed by user (non-default).
  // Requires app registry to be in a ready state.
  virtual int CountUserInstalledApps() const = 0;

  void AddObserver(AppRegistrarObserver* observer);
  void RemoveObserver(const AppRegistrarObserver* observer);

 protected:
  Profile* profile() const { return profile_; }

  void NotifyWebAppInstalled(const AppId& app_id);
  void NotifyWebAppUninstalled(const AppId& app_id);
  void NotifyAppRegistrarShutdown();

 private:
  Profile* profile_;

  base::ObserverList<AppRegistrarObserver, /*check_empty=*/true> observers_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRAR_H_
