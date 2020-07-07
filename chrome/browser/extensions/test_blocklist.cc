// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_blocklist.h"

#include <set>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/blocklist.h"
#include "chrome/browser/extensions/blocklist_state_fetcher.h"
#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"

namespace extensions {

namespace {

void Assign(BlocklistState* out, BlocklistState in) {
  *out = in;
}

}  // namespace

BlacklistStateFetcherMock::BlacklistStateFetcherMock() : request_count_(0) {}

BlacklistStateFetcherMock::~BlacklistStateFetcherMock() {}

void BlacklistStateFetcherMock::Request(const std::string& id,
                                        const RequestCallback& callback) {
  ++request_count_;

  BlocklistState result = NOT_BLOCKLISTED;
  if (base::Contains(states_, id))
    result = states_[id];

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, result));
}

void BlacklistStateFetcherMock::SetState(const std::string& id,
                                         BlocklistState state) {
  states_[id] = state;
}

void BlacklistStateFetcherMock::Clear() {
  states_.clear();
}

TestBlacklist::TestBlacklist()
    : blacklist_(nullptr),
      blacklist_db_(new FakeSafeBrowsingDatabaseManager(true)),
      scoped_blacklist_db_(blacklist_db_) {}

TestBlacklist::TestBlacklist(Blacklist* blacklist)
    : blacklist_(nullptr),
      blacklist_db_(new FakeSafeBrowsingDatabaseManager(true)),
      scoped_blacklist_db_(blacklist_db_) {
  Attach(blacklist);
}

TestBlacklist::~TestBlacklist() {
  Detach();
}

void TestBlacklist::Attach(Blacklist* blacklist) {
  if (blacklist_)
    Detach();

  blacklist_ = blacklist;
  blacklist_->SetBlacklistStateFetcherForTest(&state_fetcher_mock_);
}

void TestBlacklist::Detach() {
  blacklist_->ResetBlacklistStateFetcherForTest();
  blacklist_->ResetDatabaseUpdatedListenerForTest();
}

void TestBlacklist::SetBlacklistState(const std::string& extension_id,
                                      BlocklistState state,
                                      bool notify) {
  state_fetcher_mock_.SetState(extension_id, state);

  switch (state) {
    case NOT_BLOCKLISTED:
      blacklist_db_->RemoveUnsafe(extension_id);
      break;

    case BLOCKLISTED_MALWARE:
    case BLOCKLISTED_SECURITY_VULNERABILITY:
    case BLOCKLISTED_CWS_POLICY_VIOLATION:
    case BLOCKLISTED_POTENTIALLY_UNWANTED:
      blacklist_db_->AddUnsafe(extension_id);
      break;

    default:
      break;
  }

  if (notify)
    blacklist_db_->NotifyUpdate();
}

void TestBlacklist::Clear(bool notify) {
  state_fetcher_mock_.Clear();
  blacklist_db_->ClearUnsafe();
  if (notify)
    blacklist_db_->NotifyUpdate();
}

BlocklistState TestBlacklist::GetBlacklistState(
    const std::string& extension_id) {
  BlocklistState blacklist_state;
  blacklist_->IsBlacklisted(extension_id,
                            base::Bind(&Assign, &blacklist_state));
  base::RunLoop().RunUntilIdle();
  return blacklist_state;
}

void TestBlacklist::DisableSafeBrowsing() {
  blacklist_db_->Disable();
}

void TestBlacklist::EnableSafeBrowsing() {
  blacklist_db_->Enable();
}

void TestBlacklist::NotifyUpdate() {
  blacklist_db_->NotifyUpdate();
}

}  // namespace extensions
