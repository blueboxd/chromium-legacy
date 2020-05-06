// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UNSAFE_RESOURCE_CONTAINER_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UNSAFE_RESOURCE_CONTAINER_H_

#include "components/security_interstitials/core/unsafe_resource.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

namespace web {
class NavigationItem;
}

// Helper object that holds UnsafeResources for a WebState. UnsafeResources are
// copied and added when a navigation are detected to be unsafe, then released
// to populate the error page shown to the user after a navigation fails.
class SafeBrowsingUnsafeResourceContainer
    : public web::WebStateUserData<SafeBrowsingUnsafeResourceContainer> {
 public:
  // SafeBrowsingUnsafeResourceContainer is move-only.
  SafeBrowsingUnsafeResourceContainer(
      SafeBrowsingUnsafeResourceContainer&& other);
  SafeBrowsingUnsafeResourceContainer& operator=(
      SafeBrowsingUnsafeResourceContainer&& other);
  ~SafeBrowsingUnsafeResourceContainer() override;

  // Stores a copy of |resource|.  Only one UnsafeResource can be stored at a
  // time.
  void StoreUnsafeResource(
      const security_interstitials::UnsafeResource& resource);

  // Returns the main frame UnsafeResource, or null if one has not been stored.
  const security_interstitials::UnsafeResource* GetMainFrameUnsafeResource()
      const;

  // Returns the main frame UnsafeResource, transferring ownership to the
  // caller.  Returns null if there is no main frame unsafe resource.
  std::unique_ptr<security_interstitials::UnsafeResource>
  ReleaseMainFrameUnsafeResource();

  // Returns the sub frame UnsafeResource for |item|, or null if one has not
  // been stored.  |item| must be non-null.
  const security_interstitials::UnsafeResource* GetSubFrameUnsafeResource(
      web::NavigationItem* item) const;

  // Returns the sub frame UnsafeResource for |item|, transferring ownership to
  // the caller.  Returns null if |item| has no sub frame unsafe resources.
  // |item| must be non-null.
  std::unique_ptr<security_interstitials::UnsafeResource>
  ReleaseSubFrameUnsafeResource(web::NavigationItem* item);

 private:
  explicit SafeBrowsingUnsafeResourceContainer(web::WebState* web_state);
  friend class web::WebStateUserData<SafeBrowsingUnsafeResourceContainer>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // The WebState whose unsafe resources are managed by this container.
  web::WebState* web_state_ = nullptr;
  // The UnsafeResource for the main frame navigation.
  std::unique_ptr<security_interstitials::UnsafeResource>
      main_frame_unsafe_resource_;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UNSAFE_RESOURCE_CONTAINER_H_
