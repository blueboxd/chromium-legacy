// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_

#include "base/callback.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/browser/service_worker/service_worker_single_script_update_checker.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

class ServiceWorkerVersion;

class CONTENT_EXPORT ServiceWorkerUpdateChecker {
 public:
  // Data of each compared script needed in remaining update process
  struct CONTENT_EXPORT ComparedScriptInfo {
    ComparedScriptInfo();
    ComparedScriptInfo(
        int64_t old_resource_id,
        ServiceWorkerSingleScriptUpdateChecker::Result result,
        std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
            paused_state);
    ComparedScriptInfo(ComparedScriptInfo&& other);
    ComparedScriptInfo& operator=(ComparedScriptInfo&& other);
    ~ComparedScriptInfo();

    // Resource id of the compared script already in storage
    int64_t old_resource_id;

    // Compare result of a single script
    ServiceWorkerSingleScriptUpdateChecker::Result result;

    // This is only valid for script compared to be different. It consists of
    // some state variables to continue the update process, including
    // ServiceWorkerCacheWriter, and Mojo endpoints for downloading.
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
        paused_state;
  };

  // This is to notify the update check result. Value of |result| can be:
  // 1. ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical
  //    All the scripts are identical with existing version, no need to update.
  //    |failure_info| is nullptr.
  // 2. ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent
  //    Some script changed, update is needed. |failure_info| is nullptr.
  // 3. ServiceWorkerSingleScriptUpdateChecker::Result::kFailed
  //    The update check failed, detailed error info is in |failure_info|.
  using UpdateStatusCallback = base::OnceCallback<void(
      ServiceWorkerSingleScriptUpdateChecker::Result result,
      std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
          failure_info)>;

  ServiceWorkerUpdateChecker(
      std::vector<ServiceWorkerDatabase::ResourceRecord> scripts_to_compare,
      const GURL& main_script_url,
      int64_t main_script_resource_id,
      scoped_refptr<ServiceWorkerVersion> version_to_update,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      bool force_bypass_cache,
      blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
      base::TimeDelta time_since_last_check);
  ~ServiceWorkerUpdateChecker();

  // |callback| is always triggered when Start() finishes.
  void Start(UpdateStatusCallback callback);

  // This transfers the ownership of the check result to the caller. It only
  // contains the information about scripts which have already been compared.
  std::map<GURL, ComparedScriptInfo> TakeComparedResults();

  void OnOneUpdateCheckFinished(
      int64_t old_resource_id,
      const GURL& script_url,
      ServiceWorkerSingleScriptUpdateChecker::Result result,
      std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
          failure_info,
      std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
          paused_state);

  bool network_accessed() const { return network_accessed_; }

 private:
  void CheckOneScript(const GURL& url, const int64_t resource_id);

  std::vector<ServiceWorkerDatabase::ResourceRecord> scripts_to_compare_;
  size_t next_script_index_to_compare_ = 0;
  std::map<GURL, ComparedScriptInfo> script_check_results_;
  const GURL main_script_url_;
  const int64_t main_script_resource_id_;

  // The version which triggered this update.
  scoped_refptr<ServiceWorkerVersion> version_to_update_;

  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> running_checker_;

  UpdateStatusCallback callback_;

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  const bool force_bypass_cache_;
  const blink::mojom::ServiceWorkerUpdateViaCache update_via_cache_;
  const base::TimeDelta time_since_last_check_;

  // True if any at least one of the scripts is fetched by network.
  bool network_accessed_ = false;

  base::WeakPtrFactory<ServiceWorkerUpdateChecker> weak_factory_{this};

  DISALLOW_IMPLICIT_CONSTRUCTORS(ServiceWorkerUpdateChecker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_
