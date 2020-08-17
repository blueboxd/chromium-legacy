// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WELL_KNOWN_CHANGE_PASSWORD_STATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WELL_KNOWN_CHANGE_PASSWORD_STATE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace password_manager {

// Creates a SimpleURLLoader for a request to the non existing resource path for
// a given |origin|.
// TODO(crbug.com/927473): move method to anonymous namespace when State is
// integrated in NavigationThrottle.
std::unique_ptr<network::SimpleURLLoader>
CreateResourceRequestToWellKnownNonExistingResourceFor(const GURL& url);

// A delegate that is notified when the processing is done and its known if
// .well-known/change-password is supported.
class WellKnownChangePasswordStateDelegate {
 public:
  virtual ~WellKnownChangePasswordStateDelegate() = default;
  virtual void OnProcessingFinished(bool is_supported) = 0;
};

// Processes if .well-known/change-password is supported by a site.
class WellKnownChangePasswordState {
 public:
  explicit WellKnownChangePasswordState(
      password_manager::WellKnownChangePasswordStateDelegate* delegate);
  ~WellKnownChangePasswordState();
  // Request the status code from a path that is expected to return 404.
  void FetchNonExistingResource(
      network::SharedURLLoaderFactory* url_loader_factory,
      const GURL& origin);
  // The request to .well-known/change-password is not made by this State. To
  // get the response code for the request the owner of the state has to call
  // this method to tell the state.
  void SetChangePasswordResponseCode(int status_code);

 private:
  // Callback for the request to the "not exist" path.
  void FetchNonExistingResourceCallback(
      scoped_refptr<net::HttpResponseHeaders> headers);
  // Function is called when both requests are finished. Decides to continue or
  // redirect to homepage.
  void ContinueProcessing();
  // Checks if both requests are finished.
  bool BothRequestsFinished() const;
  // Checks the status codes and returns if change password is supported.
  bool SupportsChangePasswordUrl() const;

  WellKnownChangePasswordStateDelegate* delegate_ = nullptr;
  int non_existing_resource_response_code_ = 0;
  int change_password_response_code_ = 0;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WELL_KNOWN_CHANGE_PASSWORD_STATE_H_
