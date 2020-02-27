// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/persisted_data.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/version.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

// Uses the same pref as the update_client code.
constexpr char kPersistedDataPreference[] = "updateclientdata";

constexpr char kPV[] = "pv";
constexpr char kFP[] = "fp";

}  // namespace

namespace updater {

PersistedData::PersistedData(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
  DCHECK(pref_service_->FindPreference(kPersistedDataPreference));
}

PersistedData::~PersistedData() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

base::Version PersistedData::GetProductVersion(const std::string& id) const {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return base::Version(GetString(id, kPV));
}

void PersistedData::SetProductVersion(const std::string& id,
                                      const base::Version& pv) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(pv.IsValid());
  SetString(id, kPV, pv.GetString());
}

std::string PersistedData::GetFingerprint(const std::string& id) const {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return GetString(id, kFP);
}

void PersistedData::SetFingerprint(const std::string& id,
                                   const std::string& fingerprint) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  SetString(id, kFP, fingerprint);
}

std::vector<std::string> PersistedData::GetAppIds() const {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  // The prefs is a dictionary of dictionaries, where each inner dictionary
  // corresponds to an app:
  // {"updateclientdata":{"apps":{"{44FC7FE2-65CE-487C-93F4-EDEE46EEAAAB}":{...
  const auto* pref = pref_service_->GetDictionary(kPersistedDataPreference);
  if (!pref)
    return {};
  const auto* apps = pref->FindKey("apps");
  if (!apps || !apps->is_dict())
    return {};
  std::vector<std::string> app_ids;
  for (const auto& kv : apps->DictItems()) {
    const auto& app_id = kv.first;
    const auto pv = GetProductVersion(app_id);
    if (pv.IsValid())
      app_ids.push_back(app_id);
  }
  return app_ids;
}

std::string PersistedData::GetString(const std::string& id,
                                     const std::string& key) const {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(!base::Contains(id, '.'));  // Assume the id does not contain '.'.

  const base::DictionaryValue* dict =
      pref_service_->GetDictionary(kPersistedDataPreference);
  if (!dict)
    return {};
  std::string result;
  return dict->GetString(
             base::StringPrintf("apps.%s.%s", id.c_str(), key.c_str()), &result)
             ? result
             : std::string();
}

void PersistedData::SetString(const std::string& id,
                              const std::string& key,
                              const std::string& value) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(!base::Contains(id, '.'));  // Assume the id does not contain '.'.

  DictionaryPrefUpdate update(pref_service_, kPersistedDataPreference);
  update->SetString(base::StringPrintf("apps.%s.%s", id.c_str(), key.c_str()),
                    value);
}

}  // namespace updater
