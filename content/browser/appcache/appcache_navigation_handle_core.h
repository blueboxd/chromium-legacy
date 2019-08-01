// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_NAVIGATION_HANDLE_CORE_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_NAVIGATION_HANDLE_CORE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {

class AppCacheNavigationHandle;
class AppCacheHost;
class AppCacheServiceImpl;
class ChromeAppCacheService;

// This class is used to manage the lifetime of AppCacheHosts
// created during navigations. This class is created on the UI thread, but
// should only be accessed from the IO thread afterwards. It is the IO thread
// pendant of AppCacheNavigationHandle. See the
// AppCacheNavigationHandle header for more details about the lifetime of
// both classes.
class AppCacheNavigationHandleCore {
 public:
  AppCacheNavigationHandleCore(ChromeAppCacheService* appcache_service,
                               const base::UnguessableToken& appcache_host_id,
                               int process_id);
  ~AppCacheNavigationHandleCore();

  // Returns the raw AppCacheHost pointer. Ownership remains with this class.
  AppCacheHost* host() { return precreated_host_.get(); }

  // Initializes this instance. Should be called on the IO thread.
  void Initialize();

  // Returns the precreated AppCacheHost pointer. Ownership of the host is
  // released here.
  static std::unique_ptr<AppCacheHost> GetPrecreatedHost(
      const base::UnguessableToken& host_id);

  AppCacheServiceImpl* GetAppCacheService();

  // SetProcessId may only be called once, and only if kInvalidUniqueID was
  // passed to the AppCacheNavigationHandleCore's constructor (e.g. in a
  // scenario where NavigationHandleImpl needs to delay specifying the
  // |process_id| until ReadyToCommit time).
  void SetProcessId(int process_id);

 private:
  std::unique_ptr<AppCacheHost> precreated_host_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  const base::UnguessableToken appcache_host_id_;
  int process_id_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheNavigationHandleCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_NAVIGATION_HANDLE_CORE_H_
