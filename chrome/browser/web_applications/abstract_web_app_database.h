// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ABSTRACT_WEB_APP_DATABASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ABSTRACT_WEB_APP_DATABASE_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

class WebApp;

using Registry = std::map<AppId, std::unique_ptr<WebApp>>;

// An abstract database for the registry persistence.
// Exclusively used from the UI thread.
class AbstractWebAppDatabase {
 public:
  virtual ~AbstractWebAppDatabase() = default;

  using OnceRegistryOpenedCallback =
      base::OnceCallback<void(Registry registry)>;
  // Open existing or create new DB. Read all data and return it via callback.
  virtual void OpenDatabase(OnceRegistryOpenedCallback callback) = 0;

  using CompletionCallback = base::OnceCallback<void(bool success)>;

  using AppsToWrite = base::flat_set<const WebApp*>;

  // |OpenDatabase| must have been called and completed before using any other
  // methods. Otherwise, it fails with DCHECK.
  virtual void WriteWebApps(AppsToWrite apps, CompletionCallback callback) = 0;
  virtual void DeleteWebApps(std::vector<AppId> app_ids,
                             CompletionCallback callback) = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ABSTRACT_WEB_APP_DATABASE_H_
