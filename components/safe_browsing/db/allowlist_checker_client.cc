// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/db/allowlist_checker_client.h"

#include <memory>

#include "base/bind.h"

namespace safe_browsing {

// Static
void AllowlistCheckerClient::StartCheckCsdWhitelist(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    const GURL& url,
    base::Callback<void(bool)> callback_for_result) {
  // TODO(nparker): Maybe also call SafeBrowsingDatabaseManager::CanCheckUrl()
  if (!url.is_valid()) {
    callback_for_result.Run(true /* did_match_allowlist */);
    return;
  }

  // Make a client for each request. The caller could have several in
  // flight at once.
  std::unique_ptr<AllowlistCheckerClient> client =
      std::make_unique<AllowlistCheckerClient>(callback_for_result,
                                               database_manager);
  AsyncMatch match = database_manager->CheckCsdWhitelistUrl(url, client.get());

  switch (match) {
    case AsyncMatch::MATCH:
      callback_for_result.Run(true /* did_match_allowlist */);
      break;
    case AsyncMatch::NO_MATCH:
      callback_for_result.Run(false /* did_match_allowlist */);
      break;
    case AsyncMatch::ASYNC:
      // Client is now self-owned. When it gets called back with the result,
      // it will delete itself.
      client.release();
      break;
  }
}

AllowlistCheckerClient::AllowlistCheckerClient(
    base::Callback<void(bool)> callback_for_result,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager)
    : callback_for_result_(callback_for_result),
      database_manager_(database_manager),
      weak_factory_(this) {
  // Set a timer to fail open, i.e. call it "whitelisted", if the full
  // check takes too long.
  auto timeout_callback =
      base::Bind(&AllowlistCheckerClient::OnCheckWhitelistUrlTimeout,
                 weak_factory_.GetWeakPtr());
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kTimeoutMsec),
               timeout_callback);
}

AllowlistCheckerClient::~AllowlistCheckerClient() {}

// SafeBrowsingDatabaseMananger::Client impl
void AllowlistCheckerClient::OnCheckWhitelistUrlResult(
    bool did_match_allowlist) {
  timer_.Stop();
  callback_for_result_.Run(did_match_allowlist);
  // This method is invoked only if we're already self-owned.
  delete this;
}

void AllowlistCheckerClient::OnCheckWhitelistUrlTimeout() {
  database_manager_->CancelCheck(this);
  this->OnCheckWhitelistUrlResult(true /* did_match_allowlist */);
}

}  // namespace safe_browsing
