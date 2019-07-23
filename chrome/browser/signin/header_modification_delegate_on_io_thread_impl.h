// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_HEADER_MODIFICATION_DELEGATE_ON_IO_THREAD_IMPL_H_
#define CHROME_BROWSER_SIGNIN_HEADER_MODIFICATION_DELEGATE_ON_IO_THREAD_IMPL_H_

#include "chrome/browser/signin/header_modification_delegate.h"

class ProfileIOData;

namespace content {
class ResourceContext;
}

namespace signin {

// This class wraps the FixAccountConsistencyRequestHeader and
// ProcessAccountConsistencyResponseHeaders in the HeaderModificationDelegate
// interface.
class HeaderModificationDelegateOnIOThreadImpl
    : public HeaderModificationDelegate {
 public:
  explicit HeaderModificationDelegateOnIOThreadImpl(
      content::ResourceContext* resource_context);
  ~HeaderModificationDelegateOnIOThreadImpl() override;

  // HeaderModificationDelegate
  bool ShouldInterceptNavigation(
      content::NavigationUIData* navigation_ui_data) override;
  void ProcessRequest(ChromeRequestAdapter* request_adapter,
                      const GURL& redirect_url) override;
  void ProcessResponse(ResponseAdapter* response_adapter,
                       const GURL& redirect_url) override;

 private:
  ProfileIOData* const io_data_;

  DISALLOW_COPY_AND_ASSIGN(HeaderModificationDelegateOnIOThreadImpl);
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_HEADER_MODIFICATION_DELEGATE_ON_IO_THREAD_IMPL_H_
