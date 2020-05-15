// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEDERATED_LEARNING_FLOC_REMOTE_PERMISSION_SERVICE_H_
#define CHROME_BROWSER_FEDERATED_LEARNING_FLOC_REMOTE_PERMISSION_SERVICE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace signin {
class IdentityManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace federated_learning {

// Provides an API for querying Google servers for a signed-in user's
// floc related permission settings (e.g. sWAA, NAC, etc.).
class FlocRemotePermissionService : public KeyedService {
 public:
  // Handles all the work of making an API request. This class encapsulates
  // the entire state of the request. When an instance is destroyed, all
  // aspects of the request are cancelled.
  class Request {
   public:
    virtual ~Request();

    // Returns the response code received from the server, which will only be
    // valid if the request succeeded.
    virtual int GetResponseCode() = 0;

    // Returns the contents of the response body received from the server.
    virtual const std::string& GetResponseBody() = 0;

    // Tells the request to begin.
    virtual void Start() = 0;

   protected:
    Request();
  };

  using QueryFlocPermissionCallback = base::OnceCallback<void(bool success)>;

  using CompletionCallback = base::OnceCallback<void(Request*, bool success)>;

  FlocRemotePermissionService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~FlocRemotePermissionService() override;

  // Queries floc related permission settings - specifically the bit swaa, nac,
  // and account_type. The |callback| will be called with "true" if all 3 bits
  // are enabled.
  virtual void QueryFlocPermission(
      QueryFlocPermissionCallback callback,
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation);

 protected:
  // This function is pulled out for testing purposes.
  virtual std::unique_ptr<Request> CreateRequest(
      const GURL& url,
      CompletionCallback callback,
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation);

  // Called by |request| when a floc permission settings query has completed.
  // Unpacks the response and calls |callback|, which is the original callback
  // that was passed to QueryFlocRelatedPermissions().
  void QueryFlocPermissionCompletionCallback(
      FlocRemotePermissionService::QueryFlocPermissionCallback callback,
      FlocRemotePermissionService::Request* request,
      bool success);

 private:
  friend class FlocRemotePermissionServiceTest;

  // Stores pointer to IdentityManager instance. It must outlive the
  // FlocRemotePermissionService and can be null during tests.
  signin::IdentityManager* identity_manager_;

  // Request context getter to use.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Pending floc permission queries to be canceled if not complete by profile
  // shutdown.
  std::map<Request*, std::unique_ptr<Request>>
      pending_floc_permission_requests_;

  base::WeakPtrFactory<FlocRemotePermissionService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FlocRemotePermissionService);
};

}  // namespace federated_learning

#endif  // CHROME_BROWSER_FEDERATED_LEARNING_FLOC_REMOTE_PERMISSION_SERVICE_H_
