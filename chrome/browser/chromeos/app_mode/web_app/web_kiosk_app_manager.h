// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

class PrefRegistrySimple;

namespace chromeos {

// Does the management of web kiosk apps.
class WebKioskAppManager : public KioskAppDataDelegate {
 public:
  struct SimpleWebAppData {
    explicit SimpleWebAppData(const WebKioskAppData& data);
    SimpleWebAppData();
    SimpleWebAppData(const SimpleWebAppData& other);
    ~SimpleWebAppData();

    std::string app_id;  // web app id
    AccountId account_id;
    std::string name;
    gfx::ImageSkia icon;
    GURL url;
  };

  class WebKioskAppManagerObserver {
   public:
    virtual void OnWebKioskAppsChanged() {}

   protected:
    virtual ~WebKioskAppManagerObserver() = default;
  };

  static const char kWebKioskDictionaryName[];

  static WebKioskAppManager* Get();

  WebKioskAppManager();
  ~WebKioskAppManager() override;

  // Registers kiosk app entries in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  void GetApps(std::vector<SimpleWebAppData>* apps) const;

  void AddObserver(WebKioskAppManagerObserver* observer);
  void RemoveObserver(WebKioskAppManagerObserver* observer);

  // KioskAppDataDelegate overrides:
  void GetKioskAppIconCacheDir(base::FilePath* cache_dir) const override;
  void OnKioskAppDataChanged(const std::string& app_id) const override;
  void OnKioskAppDataLoadFailure(const std::string& app_id) const override;

  void NotifyWebKioskAppsChanged() const;

  bool current_app_was_launched_with_zero_delay() {
    return auto_launched_with_zero_delay_;
  }

 private:
  // Updates |apps_| based on CrosSettings.
  void UpdateApps();

  void ClearRemovedApps(
      const std::map<std::string, std::unique_ptr<WebKioskAppData>>& old_apps);

  std::vector<std::unique_ptr<WebKioskAppData>> apps_;
  AccountId auto_launch_account_id_;
  bool auto_launched_with_zero_delay_ = false;

  std::unique_ptr<CrosSettings::ObserverSubscription>
      local_accounts_subscription_;
  std::unique_ptr<CrosSettings::ObserverSubscription>
      local_account_auto_login_id_subscription_;

  base::ObserverList<WebKioskAppManagerObserver, true>::Unchecked observers_;

  base::WeakPtrFactory<WebKioskAppManager> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(WebKioskAppManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_MANAGER_H_
