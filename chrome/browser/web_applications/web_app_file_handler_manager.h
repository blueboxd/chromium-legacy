// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILE_HANDLER_MANAGER_H_

#include <map>
#include <vector>

#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"

class Profile;

namespace web_app {

class WebAppFileHandlerManager : public FileHandlerManager {
 public:
  explicit WebAppFileHandlerManager(Profile* profile);
  ~WebAppFileHandlerManager() override;

 protected:
  const std::vector<apps::FileHandlerInfo>* GetAllFileHandlers(
      const AppId& app_id) override;

 private:
  using FileHandlerInfos = std::map<AppId, std::vector<apps::FileHandlerInfo>>;

  // TODO(crbug.com/938103): At the moment, we have two equivalent
  // representations of these data held in memory: here, and in WebApp. If
  // GetAllFileHandlers can be modified to return a copy rather than a
  // reference, there would be no need to cache here.
  FileHandlerInfos file_handler_infos_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILE_HANDLER_MANAGER_H_
