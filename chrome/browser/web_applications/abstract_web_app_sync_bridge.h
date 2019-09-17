// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ABSTRACT_WEB_APP_SYNC_BRIDGE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ABSTRACT_WEB_APP_SYNC_BRIDGE_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

class WebApp;

using Registry = std::map<AppId, std::unique_ptr<WebApp>>;

// An abstract sync bridge for registry persistence and data updates.
// Exclusively used from the UI thread.
class AbstractWebAppSyncBridge {
 public:
  virtual ~AbstractWebAppSyncBridge() = default;

  using RegistryOpenedCallback = base::OnceCallback<void(Registry registry)>;

  using CompletionCallback = base::OnceCallback<void(bool success)>;

  using AppsToWrite = base::flat_set<const WebApp*>;

  // Open existing or create new DB. Read all data and return it via callback.
  virtual void OpenDatabase(RegistryOpenedCallback callback) = 0;

  // |OpenDatabase| must have been called and completed before using any other
  // methods. Otherwise, it fails with DCHECK.
  virtual void WriteWebApps(AppsToWrite apps, CompletionCallback callback) = 0;
  virtual void DeleteWebApps(std::vector<AppId> app_ids,
                             CompletionCallback callback) = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ABSTRACT_WEB_APP_SYNC_BRIDGE_H_
