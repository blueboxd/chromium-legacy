// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLER_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/app_shortcut_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"

class Profile;

namespace web_app {

class FileHandlerManager : public AppRegistrarObserver {
 public:
  explicit FileHandlerManager(Profile* profile);
  ~FileHandlerManager() override;

  // |registrar| is used to observe OnWebAppInstalled/Uninstalled events.
  void SetSubsystems(AppRegistrar* registrar);
  void Start();

  // Disables OS integrations, such as shortcut creation on Linux or modifying
  // the registry on Windows, to prevent side effects while testing. Note: When
  // disabled, file handling integration will not work on most operating
  // systems.
  void DisableOsIntegrationForTesting();

  // Returns |app_id|'s URL registered to handle |launch_files|'s extensions, or
  // nullopt otherwise.
  const base::Optional<GURL> GetMatchingFileHandlerURL(
      const AppId& app_id,
      const std::vector<base::FilePath>& launch_files);

  // Enables and registers OS specific file handlers for OSs that need them.
  // Currently on Chrome OS, file handlers are enabled and registered as long as
  // the app is installed.
  void EnableAndRegisterOsFileHandlers(const AppId& app_id);

  // Disables file handlers for all OSs and unregisters OS specific file
  // handlers for OSs that need them. On Chrome OS file handlers are registered
  // separately but they are still enabled and disabled here.
  void DisableAndUnregisterOsFileHandlers(const AppId& app_id);

  // Gets all enabled file handlers for |app_id|. |nullptr| if the app has no
  // enabled file handlers. Note: The lifetime of the file handlers are tied to
  // the app they belong to.
  const std::vector<apps::FileHandlerInfo>* GetEnabledFileHandlers(
      const AppId& app_id);

  // Determines whether file handling is allowed for |app_id|. This is true if
  // the FileHandlingAPI flag is enabled.
  // TODO(crbug.com/1028448): Also return true if there is a valid file handling
  // origin trial token for |app_id|.
  bool IsFileHandlingAPIAvailable(const AppId& app_id);

 protected:
  Profile* profile() const { return profile_; }
  AppRegistrar* registrar() { return registrar_; }

  // Indicates whether file handlers have been registered for an app.
  bool AreFileHandlersEnabled(const AppId& app_id) const;

  // Gets all file handlers for |app_id|. |nullptr| if the app has no file
  // handlers.
  // Note: The lifetime of the file handlers are tied to the app they belong to.
  virtual const std::vector<apps::FileHandlerInfo>* GetAllFileHandlers(
      const AppId& app_id) = 0;

 private:
  bool disable_os_integration_for_testing_ = false;

  Profile* const profile_;
  AppRegistrar* registrar_ = nullptr;

  // AppRegistrarObserver:
  void OnWebAppUninstalled(const AppId& app_id) override;
  void OnWebAppProfileWillBeDeleted(const AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;

  ScopedObserver<AppRegistrar, AppRegistrarObserver> registrar_observer_;

  DISALLOW_COPY_AND_ASSIGN(FileHandlerManager);
};

// Compute the set of file extensions specified in |file_handlers|.
std::set<std::string> GetFileExtensionsFromFileHandlers(
    const std::vector<apps::FileHandlerInfo>& file_handlers);

// Compute the set of mime types specified in |file_handlers|.
std::set<std::string> GetMimeTypesFromFileHandlers(
    const std::vector<apps::FileHandlerInfo>& file_handlers);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLER_MANAGER_H_
