// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_BOOKMARK_APP_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_BOOKMARK_APP_INSTALL_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/components/install_manager.h"

class Profile;
enum class WebappInstallSource;
struct WebApplicationInfo;

namespace content {
class WebContents;
}

namespace web_app {
class WebAppDataRetriever;
}

namespace extensions {

class BookmarkAppHelper;

// TODO(loyso): Erase this subclass together with BookmarkAppHelper.
// crbug.com/915043.
class BookmarkAppInstallManager final : public web_app::InstallManager {
 public:
  explicit BookmarkAppInstallManager(Profile* profile);
  ~BookmarkAppInstallManager() override;

  // InstallManager:
  bool CanInstallWebApp(content::WebContents* web_contents) override;
  void InstallWebAppFromManifestWithFallback(
      content::WebContents* web_contents,
      bool force_shortcut_app,
      WebappInstallSource install_source,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback) override;
  void InstallWebAppFromManifest(content::WebContents* web_contents,
                                 WebappInstallSource install_source,
                                 WebAppInstallDialogCallback dialog_callback,
                                 OnceInstallCallback callback) override;
  void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool no_network_install,
      WebappInstallSource install_source,
      OnceInstallCallback callback) override;
  void InstallWebAppWithOptions(
      content::WebContents* web_contents,
      const web_app::ExternalInstallOptions& install_options,
      OnceInstallCallback callback) override;
  void InstallOrUpdateWebAppFromSync(
      const web_app::AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) override;
  void InstallWebAppForTesting(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) override;

  using BookmarkAppHelperFactory =
      base::RepeatingCallback<std::unique_ptr<BookmarkAppHelper>(
          Profile*,
          std::unique_ptr<WebApplicationInfo>,
          content::WebContents*,
          WebappInstallSource)>;

  const BookmarkAppHelperFactory& bookmark_app_helper_factory() const {
    return bookmark_app_helper_factory_;
  }
  void SetBookmarkAppHelperFactoryForTesting(
      BookmarkAppHelperFactory bookmark_app_helper_factory);

  using DataRetrieverFactory =
      base::RepeatingCallback<std::unique_ptr<web_app::WebAppDataRetriever>()>;
  void SetDataRetrieverFactoryForTesting(
      DataRetrieverFactory data_retriever_factory);

 private:
  BookmarkAppHelperFactory bookmark_app_helper_factory_;
  DataRetrieverFactory data_retriever_factory_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_BOOKMARK_APP_INSTALL_MANAGER_H_
