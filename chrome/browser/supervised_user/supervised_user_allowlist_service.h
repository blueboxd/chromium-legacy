// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ALLOWLIST_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ALLOWLIST_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/sync/model/syncable_service.h"

class PrefService;
class SupervisedUserSiteList;

namespace base {
class DictionaryValue;
class FilePath;
}  // namespace base

namespace component_updater {
class SupervisedUserWhitelistInstaller;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace sync_pb {
class ManagedUserWhitelistSpecifics;
}

class SupervisedUserAllowlistService : public syncer::SyncableService {
 public:
  typedef base::Callback<void(
      const std::vector<scoped_refptr<SupervisedUserSiteList>>&)>
      SiteListsChangedCallback;

  SupervisedUserAllowlistService(
      PrefService* prefs,
      component_updater::SupervisedUserWhitelistInstaller* installer,
      const std::string& client_id);
  ~SupervisedUserAllowlistService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void Init();

  // Adds a callback to be called when the list of loaded site lists changes.
  // The callback will also be called immediately, to get the current
  // site lists.
  void AddSiteListsChangedCallback(const SiteListsChangedCallback& callback);

  // Returns a map (from CRX ID to name) of allowlists to be installed,
  // specified on the command line.
  static std::map<std::string, std::string> GetAllowlistsFromCommandLine();

  // Loads an already existing allowlist on disk (i.e. without downloading it as
  // a component).
  void LoadAllowlistForTesting(const std::string& id,
                               const base::string16& title,
                               const base::FilePath& path);

  // Unloads a allowlist. Public for testing.
  void UnloadAllowlist(const std::string& id);

  // Creates Sync data for a allowlist with the given |id| and |name|.
  // Public for testing.
  static syncer::SyncData CreateAllowlistSyncData(const std::string& id,
                                                  const std::string& name);

  // SyncableService implementation:
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  base::Optional<syncer::ModelError> MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  base::Optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  syncer::SyncDataList GetAllSyncDataForTesting(syncer::ModelType type) const;

 private:
  // The following methods handle allowlist additions, updates and removals,
  // usually coming from Sync.
  void AddNewAllowlist(base::DictionaryValue* pref_dict,
                       const sync_pb::ManagedUserWhitelistSpecifics& allowlist);
  void SetAllowlistProperties(
      base::DictionaryValue* pref_dict,
      const sync_pb::ManagedUserWhitelistSpecifics& allowlist);
  void RemoveAllowlist(base::DictionaryValue* pref_dict, const std::string& id);

  enum AllowlistSource {
    FROM_SYNC,
    FROM_COMMAND_LINE,
  };

  // Registers a new or existing allowlist.
  void RegisterAllowlist(const std::string& id,
                         const std::string& name,
                         AllowlistSource source);

  void GetLoadedAllowlists(
      std::vector<scoped_refptr<SupervisedUserSiteList>>* allowlists);

  void NotifyAllowlistsChanged();

  void OnAllowlistReady(const std::string& id,
                        const base::string16& title,
                        const base::FilePath& large_icon_path,
                        const base::FilePath& allowlist_path);
  void OnAllowlistLoaded(
      const std::string& id,
      const scoped_refptr<SupervisedUserSiteList>& allowlist);

  PrefService* prefs_;
  component_updater::SupervisedUserWhitelistInstaller* installer_;

  std::string client_id_;
  std::vector<SiteListsChangedCallback> site_lists_changed_callbacks_;

  // The set of registered allowlists. A allowlist might be registered but not
  // loaded yet, in which case it will not be in |loaded_allowlists_| yet.
  // On the other hand, every loaded allowlist has to be registered.
  std::set<std::string> registered_allowlists_;
  std::map<std::string, scoped_refptr<SupervisedUserSiteList>>
      loaded_allowlists_;

  base::WeakPtrFactory<SupervisedUserAllowlistService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserAllowlistService);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ALLOWLIST_SERVICE_H_
