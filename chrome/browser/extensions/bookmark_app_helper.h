// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_HELPER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

struct InstallableData;
class InstallableManager;
class Profile;
class SkBitmap;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
}  // namespace content

namespace web_app {
enum class ForInstallableSite;
class WebAppIconDownloader;
}  // namespace web_app

namespace extensions {
class CrxInstaller;
class Extension;
class ExtensionService;

// A helper class for creating bookmark apps from a WebContents.
// DEPRECATED. Use web_app::InstallManager instead. crbug.com/915043.
class BookmarkAppHelper : public content::NotificationObserver {
 public:
  typedef base::Callback<void(const Extension*, const WebApplicationInfo&)>
      CreateBookmarkAppCallback;

  // This helper class will create a bookmark app out of |web_app_info| and
  // install it to |service|. Icons will be downloaded from the URLs in
  // |web_app_info.icons| using |contents| if |contents| is not NULL.
  // All existing icons from WebApplicationInfo will also be used. The user
  // will then be prompted to edit the creation information via a bubble and
  // will have a chance to cancel the operation.
  // |install_source| indicates how the installation was triggered.
  BookmarkAppHelper(Profile* profile,
                    std::unique_ptr<WebApplicationInfo> web_app_info,
                    content::WebContents* contents,
                    WebappInstallSource install_source);
  ~BookmarkAppHelper() override;

  // Begins the asynchronous bookmark app creation.
  void Create(const CreateBookmarkAppCallback& callback);

  // If called, the installed extension will be considered policy installed.
  void set_is_policy_installed_app() { is_policy_installed_app_ = true; }

  bool is_policy_installed_app() { return is_policy_installed_app_; }

  // Forces the creation of a shortcut app instead of a PWA even if installation
  // is available.
  void set_shortcut_app_requested() { shortcut_app_requested_ = true; }

  // If called, the installed extension will be considered default installed.
  void set_is_default_app() { is_default_app_ = true; }

  bool is_default_app() { return is_default_app_; }

  // If called, the installed extension will be considered system installed.
  void set_is_system_app() { is_system_app_ = true; }

  bool is_system_app() { return is_system_app_; }

  void set_is_no_network_install() { is_no_network_install_ = true; }

  bool is_no_network_install() { return is_no_network_install_; }

  void set_skip_adding_to_applications_menu() {
    add_to_applications_menu_ = false;
  }

  bool add_to_applications_menu() { return add_to_applications_menu_; }

  // If called, desktop shortcuts will not be created. Has no effect on
  // platforms other than Linux and Windows.
  void set_skip_adding_to_desktop() { add_to_desktop_ = false; }

  bool add_to_desktop() const { return add_to_desktop_; }

  // If called, the app will not be pinned to the shelf. Has no effect on
  // platforms other than Chrome OS.
  void set_skip_adding_to_quick_launch_bar() {
    add_to_quick_launch_bar_ = false;
  }

  bool add_to_quick_launch_bar() { return add_to_quick_launch_bar_; }

  // If called, the installability check won't test for a service worker.
  void set_bypass_service_worker_check() {
    DCHECK(is_default_app() || is_system_app());
    bypass_service_worker_check_ = true;
  }

  // If called, the installation will only succeed if a manifest is found.
  void set_require_manifest() {
    DCHECK(is_default_app());
    require_manifest_ = true;
  }

  // If called, the installed app will launch in |launch_type|. User might still
  // be able to change the launch type depending on the type of app.
  void set_forced_launch_type(LaunchType launch_type) {
    forced_launch_type_ = launch_type;
  }

  const base::Optional<LaunchType>& forced_launch_type() const {
    return forced_launch_type_;
  }

 protected:
  // Protected methods for testing.

  // Called by the InstallableManager when the installability check is
  // completed.
  void OnDidPerformInstallableCheck(const InstallableData& data);

  // Performs post icon download tasks including installing the bookmark app.
  virtual void OnIconsDownloaded(
      bool success,
      const std::map<GURL, std::vector<SkBitmap>>& bitmaps);

  // Downloads icons from the given WebApplicationInfo using the given
  // WebContents.
  std::unique_ptr<web_app::WebAppIconDownloader> web_app_icon_downloader_;

 private:
  FRIEND_TEST_ALL_PREFIXES(BookmarkAppHelperTest,
                           CreateWindowedPWAIntoAppWindow);

  // Called after the bubble has been shown, and the user has either accepted or
  // the dialog was dismissed.
  void OnBubbleCompleted(bool user_accepted,
                         std::unique_ptr<WebApplicationInfo> web_app_info);

  // Called when the installation of the app is complete to perform the final
  // installation steps.
  void FinishInstallation(const Extension* extension);

  // Called when shortcut creation is complete.
  void OnShortcutCreationCompleted(const std::string& extension_id,
                                   bool shortcut_created);

  void MaybeStartIconDownload();

  // Returns true if we dispatched an asynchronous check for whether an intent
  // to the Play Store should be made, and false otherwise.
  bool DidCheckForIntentToPlayStore(const blink::Manifest& manifest);

  // Called when the asynchronous check for whether an intent to the Play Store
  // should be made returns.
  void OnDidCheckForIntentToPlayStore(const std::string& intent,
                                      bool should_intent_to_store);

  // Overridden from content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // The profile that the bookmark app is being added to.
  Profile* profile_;

  // The web contents that the bookmark app is being created for.
  content::WebContents* contents_;

  // The WebApplicationInfo that the bookmark app is being created for.
  WebApplicationInfo web_app_info_;

  // Called on app creation or failure.
  CreateBookmarkAppCallback callback_;

  // Used to install the created bookmark app.
  scoped_refptr<extensions::CrxInstaller> crx_installer_;

  content::NotificationRegistrar registrar_;

  InstallableManager* installable_manager_;

  web_app::ForInstallableSite for_installable_site_;

  base::Optional<LaunchType> forced_launch_type_;

  bool is_policy_installed_app_ = false;

  bool shortcut_app_requested_ = false;

  bool is_default_app_ = false;

  bool is_system_app_ = false;

  // If true, means that |web_app_info_| holds all the data needed for
  // installation and we should not try to fetch a manifest.
  bool is_no_network_install_ = false;

  bool add_to_applications_menu_ = true;

  bool add_to_desktop_ = true;

  bool add_to_quick_launch_bar_ = true;

  bool bypass_service_worker_check_ = false;

  bool require_manifest_ = false;

  // The mechanism via which the app creation was triggered.
  WebappInstallSource install_source_;

  // With fast tab unloading enabled, shutting down can cause BookmarkAppHelper
  // to be destroyed before the bookmark creation bubble. Use weak pointers to
  // prevent a heap-use-after free in this instance (https://crbug.com/534994).
  base::WeakPtrFactory<BookmarkAppHelper> weak_factory_{this};
};

// Creates or updates a bookmark app from the given |web_app_info|. Icons will
// be downloaded from the icon URLs provided in |web_app_info|.
// DEPRECATED. Use web_app::InstallManager instead. crbug.com/915043.
void CreateOrUpdateBookmarkApp(ExtensionService* service,
                               WebApplicationInfo* web_app_info,
                               bool is_locally_installed);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_HELPER_H_
