// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/state/tab_state_db_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/state/tab_state_db.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace {
const char kTabStateDBFolder[] = "tab_state_db";
}  // namespace

// static
TabStateDBFactory* TabStateDBFactory::GetInstance() {
  return base::Singleton<TabStateDBFactory>::get();
}

// static
TabStateDB* TabStateDBFactory::GetForProfile(Profile* profile) {
  // Incognito is currently not supported
  if (profile->IsOffTheRecord())
    return nullptr;

  return static_cast<TabStateDB*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

TabStateDBFactory::TabStateDBFactory()
    : BrowserContextKeyedServiceFactory(
          "TabStateDBKeyedService",
          BrowserContextDependencyManager::GetInstance()) {}

TabStateDBFactory::~TabStateDBFactory() = default;

KeyedService* TabStateDBFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(!profile->IsOffTheRecord());

  leveldb_proto::ProtoDatabaseProvider* proto_database_provider =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetProtoDatabaseProvider();
  base::FilePath tab_state_db_dir(
      profile->GetPath().AppendASCII(kTabStateDBFolder));
  return new TabStateDB(proto_database_provider, tab_state_db_dir);
}
