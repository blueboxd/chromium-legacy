// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_process.h"

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "weblayer/browser/download_manager_delegate_impl.h"
#include "weblayer/browser/system_network_context_manager.h"
#include "weblayer/common/weblayer_paths.h"

namespace weblayer {

namespace {
BrowserProcess* g_browser_process = nullptr;
}  // namespace

BrowserProcess::BrowserProcess() {
  g_browser_process = this;
}

BrowserProcess::~BrowserProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  g_browser_process = nullptr;

  SystemNetworkContextManager::DeleteInstance();
}

// static
BrowserProcess* BrowserProcess::GetInstance() {
  return g_browser_process;
}

void BrowserProcess::StartTearDown() {
  if (local_state_)
    local_state_->CommitPendingWrite();
}

PrefService* BrowserProcess::GetLocalState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!local_state_) {
    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();

    RegisterPrefs(pref_registry.get());

    base::FilePath path;
    CHECK(base::PathService::Get(DIR_USER_DATA, &path));
    path = path.AppendASCII("Local State");
    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(
        base::MakeRefCounted<JsonPrefStore>(path));

    {
      // Creating the prefs service may require reading the preferences from
      // disk.
      base::ScopedAllowBlocking allow_io;
      local_state_ = pref_service_factory.Create(pref_registry);
    }
  }

  return local_state_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
BrowserProcess::GetSharedURLLoaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return SystemNetworkContextManager::GetInstance()
      ->GetSharedURLLoaderFactory();
}

network_time::NetworkTimeTracker* BrowserProcess::GetNetworkTimeTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!network_time_tracker_) {
    network_time_tracker_ = std::make_unique<network_time::NetworkTimeTracker>(
        base::WrapUnique(new base::DefaultClock()),
        base::WrapUnique(new base::DefaultTickClock()), GetLocalState(),
        GetSharedURLLoaderFactory());
  }
  return network_time_tracker_.get();
}

void BrowserProcess::RegisterPrefs(PrefRegistrySimple* pref_registry) {
  network_time::NetworkTimeTracker::RegisterPrefs(pref_registry);
  pref_registry->RegisterIntegerPref(kDownloadNextIDPref, 0);
}

}  // namespace weblayer
