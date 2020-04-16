// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_whitelist_service.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#include "chrome/browser/supervised_user/supervised_user_site_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/protocol/sync.pb.h"

const char kName[] = "name";

SupervisedUserWhitelistService::SupervisedUserWhitelistService(
    PrefService* prefs,
    component_updater::SupervisedUserWhitelistInstaller* installer,
    const std::string& client_id)
    : prefs_(prefs), installer_(installer), client_id_(client_id) {
  DCHECK(prefs);
}

SupervisedUserWhitelistService::~SupervisedUserWhitelistService() {
}

// static
void SupervisedUserWhitelistService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kSupervisedUserWhitelists);
}

void SupervisedUserWhitelistService::Init() {
  const base::DictionaryValue* whitelists =
      prefs_->GetDictionary(prefs::kSupervisedUserWhitelists);
  for (base::DictionaryValue::Iterator it(*whitelists); !it.IsAtEnd();
       it.Advance()) {
    registered_whitelists_.insert(it.key());
  }
  UMA_HISTOGRAM_COUNTS_100("ManagedUsers.Whitelist.Count", whitelists->size());

  // The installer can be null in some unit tests.
  if (!installer_)
    return;

  installer_->Subscribe(
      base::Bind(&SupervisedUserWhitelistService::OnWhitelistReady,
                 weak_ptr_factory_.GetWeakPtr()));

  // Register whitelists specified on the command line.
  for (const auto& whitelist : GetWhitelistsFromCommandLine())
    RegisterWhitelist(whitelist.first, whitelist.second, FROM_COMMAND_LINE);
}

void SupervisedUserWhitelistService::AddSiteListsChangedCallback(
    const SiteListsChangedCallback& callback) {
  site_lists_changed_callbacks_.push_back(callback);

  std::vector<scoped_refptr<SupervisedUserSiteList>> whitelists;
  GetLoadedWhitelists(&whitelists);
  callback.Run(whitelists);
}

// static
std::map<std::string, std::string>
SupervisedUserWhitelistService::GetWhitelistsFromCommandLine() {
  std::map<std::string, std::string> whitelists;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string command_line_whitelists = command_line->GetSwitchValueASCII(
      switches::kInstallSupervisedUserWhitelists);
  std::vector<base::StringPiece> string_pieces =
      base::SplitStringPiece(command_line_whitelists, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const base::StringPiece& whitelist : string_pieces) {
    std::string id;
    std::string name;
    size_t separator = whitelist.find(':');
    if (separator != base::StringPiece::npos) {
      id = std::string(whitelist.substr(0, separator));
      name = std::string(whitelist.substr(separator + 1));
    } else {
      id = std::string(whitelist);
    }

    const bool result = whitelists.insert(std::make_pair(id, name)).second;
    DCHECK(result);
  }

  return whitelists;
}

void SupervisedUserWhitelistService::LoadWhitelistForTesting(
    const std::string& id,
    const base::string16& title,
    const base::FilePath& path) {
  bool result = registered_whitelists_.insert(id).second;
  DCHECK(result);
  OnWhitelistReady(id, title, base::FilePath(), path);
}

void SupervisedUserWhitelistService::UnloadWhitelist(const std::string& id) {
  bool result = registered_whitelists_.erase(id) > 0u;
  DCHECK(result);
  loaded_whitelists_.erase(id);
  NotifyWhitelistsChanged();
}

// static
syncer::SyncData SupervisedUserWhitelistService::CreateWhitelistSyncData(
    const std::string& id,
    const std::string& name) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::ManagedUserWhitelistSpecifics* whitelist =
      specifics.mutable_managed_user_whitelist();
  whitelist->set_id(id);
  whitelist->set_name(name);

  return syncer::SyncData::CreateLocalData(id, name, specifics);
}

void SupervisedUserWhitelistService::WaitUntilReadyToSync(
    base::OnceClosure done) {
  // This service handles sync events at any time.
  std::move(done).Run();
}

base::Optional<syncer::ModelError>
SupervisedUserWhitelistService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK_EQ(syncer::SUPERVISED_USER_WHITELISTS, type);

  syncer::SyncChangeList change_list;

  DictionaryPrefUpdate update(prefs_, prefs::kSupervisedUserWhitelists);
  base::DictionaryValue* pref_dict = update.Get();
  std::set<std::string> seen_ids;

  for (const syncer::SyncData& sync_data : initial_sync_data) {
    DCHECK_EQ(syncer::SUPERVISED_USER_WHITELISTS, sync_data.GetDataType());
    const sync_pb::ManagedUserWhitelistSpecifics& whitelist =
        sync_data.GetSpecifics().managed_user_whitelist();
    std::string id = whitelist.id();
    std::string name = whitelist.name();
    seen_ids.insert(id);
    base::DictionaryValue* dict = nullptr;
    if (pref_dict->GetDictionary(id, &dict)) {
      std::string old_name;
      bool result = dict->GetString(kName, &old_name);
      DCHECK(result);
      if (name != old_name) {
        SetWhitelistProperties(dict, whitelist);
      }
    } else {
      AddNewWhitelist(pref_dict, whitelist);
    }
  }

  std::set<std::string> ids_to_remove;
  for (base::DictionaryValue::Iterator it(*pref_dict); !it.IsAtEnd();
       it.Advance()) {
    if (seen_ids.find(it.key()) == seen_ids.end())
      ids_to_remove.insert(it.key());
  }

  for (const std::string& id : ids_to_remove)
    RemoveWhitelist(pref_dict, id);

  // Notify if whitelists have been uninstalled. We will notify about newly
  // added whitelists later, when they are actually available
  // (in OnWhitelistLoaded).
  if (!ids_to_remove.empty())
    NotifyWhitelistsChanged();

  // The function does not generate any errors, so it can always return
  // base::nullopt.
  return base::nullopt;
}

void SupervisedUserWhitelistService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(syncer::SUPERVISED_USER_WHITELISTS, type);
}

syncer::SyncDataList SupervisedUserWhitelistService::GetAllSyncDataForTesting(
    syncer::ModelType type) const {
  syncer::SyncDataList sync_data;
  const base::DictionaryValue* whitelists =
      prefs_->GetDictionary(prefs::kSupervisedUserWhitelists);
  for (base::DictionaryValue::Iterator it(*whitelists); !it.IsAtEnd();
       it.Advance()) {
    const std::string& id = it.key();
    const base::DictionaryValue* dict = nullptr;
    it.value().GetAsDictionary(&dict);
    std::string name;
    bool result = dict->GetString(kName, &name);
    DCHECK(result);
    sync_pb::EntitySpecifics specifics;
    sync_pb::ManagedUserWhitelistSpecifics* whitelist =
        specifics.mutable_managed_user_whitelist();
    whitelist->set_id(id);
    whitelist->set_name(name);
    sync_data.push_back(syncer::SyncData::CreateLocalData(id, name, specifics));
  }
  return sync_data;
}

base::Optional<syncer::ModelError>
SupervisedUserWhitelistService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  bool whitelists_removed = false;
  DictionaryPrefUpdate update(prefs_, prefs::kSupervisedUserWhitelists);
  base::DictionaryValue* pref_dict = update.Get();
  for (const syncer::SyncChange& sync_change : change_list) {
    syncer::SyncData data = sync_change.sync_data();
    DCHECK_EQ(syncer::SUPERVISED_USER_WHITELISTS, data.GetDataType());
    const sync_pb::ManagedUserWhitelistSpecifics& whitelist =
        data.GetSpecifics().managed_user_whitelist();
    std::string id = whitelist.id();
    switch (sync_change.change_type()) {
      case syncer::SyncChange::ACTION_ADD: {
        DCHECK(!pref_dict->HasKey(id)) << id;
        AddNewWhitelist(pref_dict, whitelist);
        break;
      }
      case syncer::SyncChange::ACTION_UPDATE: {
        base::DictionaryValue* dict = nullptr;
        pref_dict->GetDictionaryWithoutPathExpansion(id, &dict);
        SetWhitelistProperties(dict, whitelist);
        break;
      }
      case syncer::SyncChange::ACTION_DELETE: {
        DCHECK(pref_dict->HasKey(id)) << id;
        RemoveWhitelist(pref_dict, id);
        whitelists_removed = true;
        break;
      }
      case syncer::SyncChange::ACTION_INVALID: {
        NOTREACHED();
        break;
      }
    }
  }

  if (whitelists_removed)
    NotifyWhitelistsChanged();

  return base::nullopt;
}

void SupervisedUserWhitelistService::AddNewWhitelist(
    base::DictionaryValue* pref_dict,
    const sync_pb::ManagedUserWhitelistSpecifics& whitelist) {
  base::RecordAction(base::UserMetricsAction("ManagedUsers_Whitelist_Added"));

  RegisterWhitelist(whitelist.id(), whitelist.name(), FROM_SYNC);
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  SetWhitelistProperties(dict.get(), whitelist);
  pref_dict->SetWithoutPathExpansion(whitelist.id(), std::move(dict));
}

void SupervisedUserWhitelistService::SetWhitelistProperties(
    base::DictionaryValue* dict,
    const sync_pb::ManagedUserWhitelistSpecifics& whitelist) {
  dict->SetString(kName, whitelist.name());
}

void SupervisedUserWhitelistService::RemoveWhitelist(
    base::DictionaryValue* pref_dict,
    const std::string& id) {
  base::RecordAction(base::UserMetricsAction("ManagedUsers_Whitelist_Removed"));

  pref_dict->RemoveWithoutPathExpansion(id, NULL);
  installer_->UnregisterWhitelist(client_id_, id);
  UnloadWhitelist(id);
}

void SupervisedUserWhitelistService::RegisterWhitelist(const std::string& id,
                                                       const std::string& name,
                                                       WhitelistSource source) {
  bool result = registered_whitelists_.insert(id).second;
  DCHECK(result);

  // Using an empty client ID for whitelists installed from the command line
  // causes the installer to not persist the installation, so the whitelist will
  // be removed the next time the browser is started without the command line
  // flag.
  installer_->RegisterWhitelist(
      source == FROM_COMMAND_LINE ? std::string() : client_id_, id, name);
}

void SupervisedUserWhitelistService::GetLoadedWhitelists(
    std::vector<scoped_refptr<SupervisedUserSiteList>>* whitelists) {
  for (const auto& whitelist : loaded_whitelists_)
    whitelists->push_back(whitelist.second);
}

void SupervisedUserWhitelistService::NotifyWhitelistsChanged() {
  std::vector<scoped_refptr<SupervisedUserSiteList>> whitelists;
  GetLoadedWhitelists(&whitelists);

  for (const auto& callback : site_lists_changed_callbacks_)
    callback.Run(whitelists);
}

void SupervisedUserWhitelistService::OnWhitelistReady(
    const std::string& id,
    const base::string16& title,
    const base::FilePath& large_icon_path,
    const base::FilePath& whitelist_path) {
  // If we did not register the whitelist or it has been unregistered in the
  // mean time, ignore it.
  if (registered_whitelists_.count(id) == 0u)
    return;

  SupervisedUserSiteList::Load(
      id, title, large_icon_path, whitelist_path,
      base::Bind(&SupervisedUserWhitelistService::OnWhitelistLoaded,
                 weak_ptr_factory_.GetWeakPtr(), id, base::TimeTicks::Now()));
}

void SupervisedUserWhitelistService::OnWhitelistLoaded(
    const std::string& id,
    base::TimeTicks start_time,
    const scoped_refptr<SupervisedUserSiteList>& whitelist) {
  if (!whitelist) {
    LOG(WARNING) << "Couldn't load whitelist " << id;
    return;
  }

  UMA_HISTOGRAM_TIMES("ManagedUsers.Whitelist.TotalLoadDuration",
                      base::TimeTicks::Now() - start_time);

  // If the whitelist has been unregistered in the mean time, ignore it.
  if (registered_whitelists_.count(id) == 0u)
    return;

  loaded_whitelists_[id] = whitelist;
  NotifyWhitelistsChanged();
}
