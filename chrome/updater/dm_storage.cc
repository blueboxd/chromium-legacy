// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/dm_storage.h"

#include <set>
#include <utility>

#include "base/base64.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/dm_cached_policy_info.h"
#include "chrome/updater/dm_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace updater {

namespace {

// This DM Token value is persisted if the server asks the client to invalidate
// the DM Token.
constexpr char kInvalidTokenValue[] = "INVALID_DM_TOKEN";

// This is the standard name for the file that PersistPolicies() uses to store
// a PolicyFetchResponse received from the DMServer during the previous request.
// The data within the PolicyFetchResponse, such as the public key, version, and
// timestamp are used for subsequent requests and validations of DMServer
// responses.
constexpr char kPolicyInfoFileName[] = "CachedPolicyInfo";

// This is the standard name for the file that PersistPolicies() uses for each
// {policy_type} that it receives from the DMServer.
constexpr char kPolicyFileName[] = "PolicyFetchResponse";

// The policy type for Omaha policy settings.
constexpr char kGoogleUpdatePolicyType[] = "google/machine-level-omaha";

// Deletes the child directories in cache root if they do not appear in
// set |policy_types_base64|.
bool DeleteObsoletePolicies(const base::FilePath& cache_root,
                            const std::set<std::string>& policy_types_base64) {
  bool result = true;
  base::FileEnumerator cached_files(cache_root,
                                    /* recursive */ false,
                                    base::FileEnumerator::DIRECTORIES,
                                    FILE_PATH_LITERAL("*"));
  for (base::FilePath file = cached_files.Next(); !file.empty();
       file = cached_files.Next()) {
    const std::string file_base_name = file.BaseName().MaybeAsASCII();
    if (policy_types_base64.count(file_base_name))
      continue;

    if (!base::DeleteFileRecursively(file))
      result = false;
  }

  return result;
}

}  // namespace

DMStorage::DMStorage(const base::FilePath& policy_cache_root,
                     std::unique_ptr<TokenServiceInterface> token_service)
    : policy_cache_root_(policy_cache_root),
      token_service_(std::move(token_service)) {
  DCHECK(token_service_);
}

DMStorage::~DMStorage() = default;

bool DMStorage::DeregisterDevice() {
  return token_service_->StoreDmToken(kInvalidTokenValue);
}

bool DMStorage::IsValidDMToken() {
  std::string dm_token = GetDmToken();
  return !dm_token.empty() && dm_token != kInvalidTokenValue;
}

bool DMStorage::PersistPolicies(const std::string& policy_info_data,
                                const DMPolicyMap& policy_map) const {
  // Persists policy cached info
  base::FilePath policy_info_file =
      policy_cache_root_.AppendASCII(kPolicyInfoFileName);
  if (!base::ImportantFileWriter::WriteFileAtomically(policy_info_file,
                                                      policy_info_data)) {
    return false;
  }

  // Persists individual policies.
  std::set<std::string> policy_types_base64;
  for (const auto& policy_entry : policy_map) {
    const std::string& policy_type = policy_entry.first;
    const std::string& policy_value = policy_entry.second;

    std::string encoded_policy_type;
    base::Base64Encode(policy_type, &encoded_policy_type);

    policy_types_base64.emplace(encoded_policy_type);

    base::FilePath policy_dir =
        policy_cache_root_.AppendASCII(encoded_policy_type);
    if (!base::CreateDirectory(policy_dir))
      return false;
    base::FilePath policy_file = policy_dir.AppendASCII(kPolicyFileName);
    if (!base::ImportantFileWriter::WriteFileAtomically(policy_file,
                                                        policy_value)) {
      return false;
    }
  }

  // Purge all stale policies not in |policy_types_base64|.
  return DeleteObsoletePolicies(policy_cache_root_, policy_types_base64);
}

std::unique_ptr<CachedPolicyInfo> DMStorage::GetCachedPolicyInfo() {
  if (!IsValidDMToken())
    return nullptr;

  base::FilePath policy_info_file =
      policy_cache_root_.AppendASCII(kPolicyInfoFileName);
  std::string policy_info_data;
  auto cached_info = std::make_unique<CachedPolicyInfo>();
  if (!base::PathExists(policy_info_file) ||
      !base::ReadFileToString(policy_info_file, &policy_info_data) ||
      !cached_info->Populate(policy_info_data)) {
    return nullptr;
  }

  return cached_info;
}

std::unique_ptr<PolicyManagerInterface> DMStorage::GetOmahaPolicyManager() {
  if (!IsValidDMToken())
    return nullptr;

  std::string encoded_omaha_policy_type;
  base::Base64Encode(kGoogleUpdatePolicyType, &encoded_omaha_policy_type);

  base::FilePath omaha_policy_file =
      policy_cache_root_.AppendASCII(encoded_omaha_policy_type)
          .AppendASCII(kPolicyFileName);
  std::string response_data;
  ::enterprise_management::PolicyFetchResponse response;
  ::enterprise_management::PolicyData policy_data;
  ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings;
  if (!base::PathExists(omaha_policy_file) ||
      !base::ReadFileToString(omaha_policy_file, &response_data) ||
      response_data.empty() || !response.ParseFromString(response_data) ||
      !policy_data.ParseFromString(response.policy_data()) ||
      !policy_data.has_policy_value() ||
      !omaha_settings.ParseFromString(policy_data.policy_value())) {
    return nullptr;
  }

  return std::make_unique<DMPolicyManager>(omaha_settings);
}

}  // namespace updater
