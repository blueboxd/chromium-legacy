// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CROWD_DENY_SAFE_BROWSING_REQUEST_H_
#define CHROME_BROWSER_PERMISSIONS_CROWD_DENY_SAFE_BROWSING_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/public/browser/browser_thread.h"

namespace url {
class Origin;
}

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}

// Represents a single request to the Safe Browsing service to fetch the crowd
// deny verdict for a given origin. Can be created and used on any one thread.
class CrowdDenySafeBrowsingRequest {
 public:
  // The crowd deny verdict for a given origin.
  enum class Verdict {
    kAcceptable,
    kKnownToShowUnsolicitedNotificationPermissionRequests
  };

  using VerdictCallback = base::OnceCallback<void(Verdict)>;

  // Constructs a request that fetches the verdict for |origin| by consulting
  // the |database_manager|, and invokes |callback| when done.
  //
  // It is guaranteed that |callback| will never be invoked synchronously, and
  // it will not be invoked after |this| goes out of scope.
  CrowdDenySafeBrowsingRequest(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      const url::Origin& origin,
      VerdictCallback callback);
  ~CrowdDenySafeBrowsingRequest();

 private:
  class SafeBrowsingClient;

  CrowdDenySafeBrowsingRequest(const CrowdDenySafeBrowsingRequest&) = delete;
  CrowdDenySafeBrowsingRequest& operator=(const CrowdDenySafeBrowsingRequest&) =
      delete;

  // Posted by the |client_| from the IO thread when it gets a response.
  void OnReceivedResult(Verdict verdict);

  // The client interfacing with Safe Browsing. Created on |this| thread, but
  // used on the IO thread for the rest of its life and destroyed there.
  std::unique_ptr<SafeBrowsingClient> client_;

  VerdictCallback callback_;
  base::WeakPtrFactory<CrowdDenySafeBrowsingRequest> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_CROWD_DENY_SAFE_BROWSING_REQUEST_H_
