// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/feature_list_creator.h"

#include "base/base_switches.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"

namespace weblayer {

namespace {

void HandleReadError(PersistentPrefStore::PrefReadError error) {}

#if defined(OS_ANDROID)
base::FilePath GetPrefStorePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path);
  path = path.Append(FILE_PATH_LITERAL("pref_store"));
  return path;
}
#endif

std::unique_ptr<PrefService> CreatePrefService() {
  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();

#if defined(OS_ANDROID)
  metrics::AndroidMetricsServiceClient::RegisterPrefs(pref_registry.get());
#endif
  // TODO(weblayer-dev): Register prefs with VariationsService
  PrefServiceFactory pref_service_factory;

#if defined(OS_ANDROID)
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<JsonPrefStore>(GetPrefStorePath()));
#else
  // For now just use in memory PrefStore for desktop.
  // TODO(weblayer-dev): Find a long term solution.
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<InMemoryPrefStore>());
#endif

  pref_service_factory.set_read_error_callback(
      base::BindRepeating(&HandleReadError));

  return pref_service_factory.Create(pref_registry);
}

}  // namespace

FeatureListCreator::FeatureListCreator() = default;

FeatureListCreator::~FeatureListCreator() = default;

void FeatureListCreator::CreateLocalState() {
  local_state_ = CreatePrefService();
#if defined(OS_ANDROID)
  WebLayerMetricsServiceClient::GetInstance()->Initialize(local_state_.get());
#endif
}

}  // namespace weblayer
