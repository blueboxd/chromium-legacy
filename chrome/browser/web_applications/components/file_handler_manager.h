// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLER_MANAGER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"

class Profile;

namespace web_app {

class FileHandlerManager {
 public:
  explicit FileHandlerManager(Profile* profile) : profile_(profile) {}
  virtual ~FileHandlerManager() = default;

  // Gets all file handlers for |app_id|. |nullptr| if the app has no file
  // handlers.
  // Note: The lifetime of the file handlers are tied to the app they belong to.
  virtual const std::vector<apps::FileHandlerInfo>* GetFileHandlers(
      const AppId& app_id) = 0;

 protected:
  Profile* profile() { return profile_; }

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(FileHandlerManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLER_MANAGER_H_
