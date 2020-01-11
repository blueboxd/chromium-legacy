// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_process.h"

#include "base/memory/ref_counted.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "weblayer/browser/system_network_context_manager.h"

namespace weblayer {

namespace {
BrowserProcess* g_browser_process = nullptr;

// Creates the PrefService that will be used as the browser process's local
// state.
std::unique_ptr<PrefService> CreatePrefService() {
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();

  network_time::NetworkTimeTracker::RegisterPrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<InMemoryPrefStore>());

  return pref_service_factory.Create(pref_registry);
}

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

PrefService* BrowserProcess::GetLocalState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!local_state_)
    local_state_ = CreatePrefService();

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

}  // namespace weblayer
