// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PERSISTED_DATA_H_
#define CHROME_UPDATER_PERSISTED_DATA_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"

class PrefService;

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace updater {

struct RegistrationRequest;

// PersistedData uses the PrefService to persist updater data that outlives
// the updater processes.
//
// This class has sequence affinity.
//
// A mechanism to remove apps or app versions from prefs is needed.
// TODO(sorin): crbug.com/1056450
class PersistedData : public base::RefCounted<PersistedData> {
 public:
  // Constructs a provider using the specified |pref_service|.
  // The associated preferences are assumed to already be registered.
  // The |pref_service| must outlive the instance of this class.
  explicit PersistedData(PrefService* pref_service);
  PersistedData(const PersistedData&) = delete;
  PersistedData& operator=(const PersistedData&) = delete;

  // These functions access |pv| data for the specified |id|. Returns an empty
  // version, if the version is not found.
  base::Version GetProductVersion(const std::string& id) const;
  void SetProductVersion(const std::string& id, const base::Version& pv);

  // These functions access |fingerprint| data for the specified |id|.
  std::string GetFingerprint(const std::string& id) const;
  void SetFingerprint(const std::string& id, const std::string& fp);

  // These functions access the existence checker path for the specified id.
  base::FilePath GetExistenceCheckerPath(const std::string& id) const;
  void SetExistenceCheckerPath(const std::string& id,
                               const base::FilePath& ecp);

  // These functions access the brand code for the specified id.
  std::string GetBrandCode(const std::string& id) const;
  void SetBrandCode(const std::string& id, const std::string& bc);

  // These functions access the tag for the specified id.
  std::string GetTag(const std::string& id) const;
  void SetTag(const std::string& id, const std::string& tag);

  // This function sets everything in the registration request object into the
  // persistent data store.
  void RegisterApp(const RegistrationRequest& rq);

  // Returns the app ids of the applications registered in prefs, if the
  // application has a valid version.
  std::vector<std::string> GetAppIds() const;

 private:
  friend class base::RefCounted<PersistedData>;
  ~PersistedData();

  std::string GetString(const std::string& id, const std::string& key) const;
  void SetString(const std::string& id,
                 const std::string& key,
                 const std::string& value);

  SEQUENCE_CHECKER(sequence_checker_);

  PrefService* pref_service_ = nullptr;  // Not owned by this class.
};

}  // namespace updater

#endif  // CHROME_UPDATER_PERSISTED_DATA_H_
