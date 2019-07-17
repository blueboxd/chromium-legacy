// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_update_checker.h"

#include "base/bind.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"

namespace content {

ServiceWorkerUpdateChecker::ServiceWorkerUpdateChecker(
    std::vector<ServiceWorkerDatabase::ResourceRecord> scripts_to_compare,
    const GURL& main_script_url,
    int64_t main_script_resource_id,
    scoped_refptr<ServiceWorkerVersion> version_to_update,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    bool force_bypass_cache,
    blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
    base::TimeDelta time_since_last_check)
    : scripts_to_compare_(std::move(scripts_to_compare)),
      main_script_url_(main_script_url),
      main_script_resource_id_(main_script_resource_id),
      version_to_update_(std::move(version_to_update)),
      loader_factory_(std::move(loader_factory)),
      force_bypass_cache_(force_bypass_cache),
      update_via_cache_(update_via_cache),
      time_since_last_check_(time_since_last_check) {}

ServiceWorkerUpdateChecker::~ServiceWorkerUpdateChecker() = default;

void ServiceWorkerUpdateChecker::Start(UpdateStatusCallback callback) {
  DCHECK(!scripts_to_compare_.empty());
  callback_ = std::move(callback);
  CheckOneScript(main_script_url_, main_script_resource_id_);
}

void ServiceWorkerUpdateChecker::OnOneUpdateCheckFinished(
    int64_t old_resource_id,
    const GURL& script_url,
    ServiceWorkerSingleScriptUpdateChecker::Result result,
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
        failure_info,
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
        paused_state) {
  bool is_main_script = script_url == main_script_url_;
  // We only cares about the failures on the main script because an imported
  // script might not exist anymore and fail to be loaded because it's not
  // imported in a new script.
  // See also https://github.com/w3c/ServiceWorker/issues/1374 for more details.
  if (is_main_script &&
      result == ServiceWorkerSingleScriptUpdateChecker::Result::kFailed) {
    std::move(callback_).Run(
        ServiceWorkerSingleScriptUpdateChecker::Result::kFailed,
        std::move(failure_info));
    return;
  }

  script_check_results_[script_url] =
      ComparedScriptInfo(old_resource_id, result, std::move(paused_state));
  if (running_checker_->network_accessed())
    network_accessed_ = true;

  running_checker_.reset();

  if (ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent == result) {
    // Found an updated script. Stop the comparison of scripts here and
    // return to ServiceWorkerRegisterJob to continue the update.
    // Note that running |callback_| will delete |this|.
    std::move(callback_).Run(
        ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent,
        nullptr /* failure_info */);
    return;
  }

  if (next_script_index_to_compare_ >= scripts_to_compare_.size()) {
    // None of scripts had any updates.
    // Running |callback_| will delete |this|.
    std::move(callback_).Run(
        ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical,
        nullptr /* failure_info */);
    return;
  }

  // The main script should be skipped since it should be compared first.
  if (scripts_to_compare_[next_script_index_to_compare_].url ==
      main_script_url_) {
    next_script_index_to_compare_++;
    if (next_script_index_to_compare_ >= scripts_to_compare_.size()) {
      // None of scripts had any updates.
      // Running |callback_| will delete |this|.
      std::move(callback_).Run(
          ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical,
          nullptr /* failure_info */);
      return;
    }
  }

  const GURL& next_url = scripts_to_compare_[next_script_index_to_compare_].url;
  int64_t next_resource_id =
      scripts_to_compare_[next_script_index_to_compare_].resource_id;
  next_script_index_to_compare_++;
  CheckOneScript(next_url, next_resource_id);
}

std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo>
ServiceWorkerUpdateChecker::TakeComparedResults() {
  return std::move(script_check_results_);
}

void ServiceWorkerUpdateChecker::CheckOneScript(const GURL& url,
                                                const int64_t resource_id) {
  DCHECK_NE(ServiceWorkerConsts::kInvalidServiceWorkerResourceId, resource_id)
      << "All the target scripts should be stored in the storage.";

  bool is_main_script = url == main_script_url_;
  ServiceWorkerStorage* storage = version_to_update_->context()->storage();

  // We need two identical readers for comparing and reading the resource for
  // |resource_id| from the storage.
  auto compare_reader = storage->CreateResponseReader(resource_id);
  auto copy_reader = storage->CreateResponseReader(resource_id);

  auto writer = storage->CreateResponseWriter(storage->NewResourceId());
  running_checker_ = std::make_unique<ServiceWorkerSingleScriptUpdateChecker>(
      url, is_main_script, version_to_update_->scope(), force_bypass_cache_,
      update_via_cache_, time_since_last_check_, loader_factory_,
      std::move(compare_reader), std::move(copy_reader), std::move(writer),
      base::BindOnce(&ServiceWorkerUpdateChecker::OnOneUpdateCheckFinished,
                     weak_factory_.GetWeakPtr(), resource_id));
}

ServiceWorkerUpdateChecker::ComparedScriptInfo::ComparedScriptInfo() = default;

ServiceWorkerUpdateChecker::ComparedScriptInfo::ComparedScriptInfo(
    int64_t old_resource_id,
    ServiceWorkerSingleScriptUpdateChecker::Result result,
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
        paused_state)
    : old_resource_id(old_resource_id),
      result(result),
      paused_state(std::move(paused_state)) {}

ServiceWorkerUpdateChecker::ComparedScriptInfo::~ComparedScriptInfo() = default;

ServiceWorkerUpdateChecker::ComparedScriptInfo::ComparedScriptInfo(
    ServiceWorkerUpdateChecker::ComparedScriptInfo&& other) = default;

ServiceWorkerUpdateChecker::ComparedScriptInfo&
ServiceWorkerUpdateChecker::ComparedScriptInfo::operator=(
    ServiceWorkerUpdateChecker::ComparedScriptInfo&& other) = default;
}  // namespace content
