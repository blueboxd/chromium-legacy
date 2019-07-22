// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;
class Profile;
struct WebApplicationInfo;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
}

namespace web_app {

class InstallFinalizer;
class WebAppDataRetriever;

class WebAppInstallTask : content::WebContentsObserver {
 public:
  WebAppInstallTask(Profile* profile,
                    InstallFinalizer* install_finalizer,
                    std::unique_ptr<WebAppDataRetriever> data_retriever);
  ~WebAppInstallTask() override;

  // Checks a WebApp installability, retrieves manifest and icons and
  // then performs the actual installation.
  void InstallWebAppFromManifest(
      content::WebContents* web_contents,
      WebappInstallSource install_source,
      InstallManager::WebAppInstallDialogCallback dialog_callback,
      InstallManager::OnceInstallCallback callback);

  // This method infers WebApp info from the blink renderer process
  // and then retrieves a manifest in a way similar to
  // |InstallWebAppFromManifest|. If manifest is incomplete or missing, the
  // inferred info is used.
  void InstallWebAppFromManifestWithFallback(
      content::WebContents* web_contents,
      bool force_shortcut_app,
      WebappInstallSource install_source,
      InstallManager::WebAppInstallDialogCallback dialog_callback,
      InstallManager::OnceInstallCallback callback);

  // Starts a web app installation process using prefilled
  // |web_application_info| which holds all the data needed for installation.
  // InstallManager doesn't fetch a manifest. If |no_network_install| is true,
  // the app will not be synced, since if the data is locally available we
  // assume there is an external sync mechanism.
  void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool no_network_install,
      WebappInstallSource install_source,
      InstallManager::OnceInstallCallback callback);

  // Starts a background web app installation process for a given
  // |web_contents|. This method infers WebApp info from the blink renderer
  // process and then retrieves a manifest in a way similar to
  // |InstallWebAppFromManifestWithFallback|.
  void InstallWebAppWithOptions(content::WebContents* web_contents,
                                const ExternalInstallOptions& install_options,
                                InstallManager::OnceInstallCallback callback);

  // Starts background installation of a web app: does not show UI dialog.
  // |web_application_info| contains all the data needed for installation. Icons
  // will be downloaded from the icon URLs provided in |web_application_info|.
  void InstallWebAppFromInfoRetrieveIcons(
      content::WebContents* web_contents,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool is_locally_installed,
      WebappInstallSource install_source,
      InstallManager::OnceInstallCallback callback);

  // WebContentsObserver:
  void WebContentsDestroyed() override;

  void SetInstallFinalizerForTesting(InstallFinalizer* install_finalizer);

 private:
  void CheckInstallPreconditions();
  void RecordInstallEvent(ForInstallableSite for_installable_site);

  // Calling the callback may destroy |this| task. Callers shoudln't work with
  // any |this| class members after calling it.
  void CallInstallCallback(const AppId& app_id, InstallResultCode code);

  // Checks if any errors occurred while |this| was async awaiting. All On*
  // completion handlers below must return early if this is true. Also, if
  // ShouldStopInstall is true, install_callback_ is already invoked or may be
  // invoked later: All On* completion handlers don't need to call
  // install_callback_.
  bool ShouldStopInstall() const;

  void OnGetWebApplicationInfo(
      bool force_shortcut_app,
      std::unique_ptr<WebApplicationInfo> web_app_info);
  void OnDidPerformInstallableCheck(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      bool force_shortcut_app,
      const blink::Manifest& manifest,
      bool valid_manifest_for_web_app,
      bool is_installable);

  // Either dispatches an asynchronous check for whether this installation
  // should be stopped and
  void CheckForPlayStoreIntentOrGetIcons(
      const blink::Manifest& manifest,
      std::unique_ptr<WebApplicationInfo> web_app_info,
      std::vector<GURL> icon_urls,
      ForInstallableSite for_installable_site,
      bool skip_page_favicons);

  // Called when the asynchronous check for whether an intent to the Play Store
  // should be made returns.
  void OnDidCheckForIntentToPlayStore(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      std::vector<GURL> icon_urls,
      ForInstallableSite for_installable_site,
      bool skip_page_favicons,
      const std::string& intent,
      bool should_intent_to_store);

  void OnIconsRetrieved(std::unique_ptr<WebApplicationInfo> web_app_info,
                        bool is_locally_installed,
                        IconsMap icons_map);
  void OnIconsRetrievedShowDialog(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      ForInstallableSite for_installable_site,
      IconsMap icons_map);
  void OnDialogCompleted(ForInstallableSite for_installable_site,
                         bool user_accepted,
                         std::unique_ptr<WebApplicationInfo> web_app_info);
  void OnInstallFinalized(const AppId& app_id, InstallResultCode code);
  void OnInstallFinalizedCreateShortcuts(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      const AppId& app_id,
      InstallResultCode code);
  void OnShortcutsCreated(std::unique_ptr<WebApplicationInfo> web_app_info,
                          const AppId& app_id,
                          bool shortcut_created);

  InstallManager::WebAppInstallDialogCallback dialog_callback_;
  InstallManager::OnceInstallCallback install_callback_;
  base::Optional<ExternalInstallOptions> install_options_;
  bool background_installation_ = false;

  // The mechanism via which the app creation was triggered.
  static constexpr WebappInstallSource kNoInstallSource =
      WebappInstallSource::COUNT;
  WebappInstallSource install_source_ = kNoInstallSource;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  InstallFinalizer* install_finalizer_;
  Profile* profile_;

  base::WeakPtrFactory<WebAppInstallTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallTask);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_
