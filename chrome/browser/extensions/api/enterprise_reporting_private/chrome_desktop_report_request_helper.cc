// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"

#if defined(OS_WIN)
#include <windows.h>
#include <dpapi.h>
#endif

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/prefs.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#endif

#if defined(OS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

#if defined(OS_MACOSX)
#include "crypto/apple_keychain.h"
#endif

namespace em = enterprise_management;

namespace extensions {
namespace {

// JSON keys in the extension arguments.
const char kBrowserReport[] = "browserReport";
const char kChromeUserProfileReport[] = "chromeUserProfileReport";
const char kChromeSignInUser[] = "chromeSignInUser";
const char kExtensionData[] = "extensionData";
const char kPlugins[] = "plugins";
const char kSafeBrowsingWarnings[] = "safeBrowsingWarnings";
const char kSafeBrowsingWarningsClickThrough[] =
    "safeBrowsingWarningsClickThrough";

// JSON keys in the os_info field.
const char kOS[] = "os";
const char kOSArch[] = "arch";
const char kOSVersion[] = "os_version";

const char kDefaultDictionary[] = "{}";
const char kDefaultList[] = "[]";

enum Type {
  LIST,
  DICTIONARY,
};

std::string GetChromePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  return path.AsUTF8Unsafe();
}

std::string GetProfileId(const Profile* profile) {
  return profile->GetOriginalProfile()->GetPath().AsUTF8Unsafe();
}

// Returns last policy fetch timestamp of machine level user cloud policy if
// it exists. Otherwise, returns zero.
int64_t GetMachineLevelUserCloudPolicyFetchTimestamp() {
  policy::MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  if (!manager || !manager->IsClientRegistered())
    return 0;
  return manager->core()->client()->last_policy_timestamp().ToJavaTime();
}

void AppendAdditionalBrowserInformation(em::ChromeDesktopReportRequest* request,
                                        Profile* profile) {
  const PrefService* prefs = profile->GetPrefs();

  // Set Chrome version number
  request->mutable_browser_report()->set_browser_version(
      version_info::GetVersionNumber());
  // Set Chrome channel
  request->mutable_browser_report()->set_channel(
      policy::ConvertToProtoChannel(chrome::GetChannel()));

  // Add a new profile report if extension doesn't report any profile.
  if (request->browser_report().chrome_user_profile_reports_size() == 0)
    request->mutable_browser_report()->add_chrome_user_profile_reports();

  DCHECK_EQ(1, request->browser_report().chrome_user_profile_reports_size());

  // Set Chrome executable path
  request->mutable_browser_report()->set_executable_path(GetChromePath());

  // Set profile ID for the first profile.
  request->mutable_browser_report()
      ->mutable_chrome_user_profile_reports(0)
      ->set_id(GetProfileId(profile));

  // Set the profile name
  request->mutable_browser_report()
      ->mutable_chrome_user_profile_reports(0)
      ->set_name(prefs->GetString(prefs::kProfileName));

  if (prefs->GetBoolean(enterprise_reporting::kReportPolicyData)) {
    // Set policy data of the first profile. Extension will report this data in
    // the future.
    auto client =
        std::make_unique<policy::ChromePolicyConversionsClient>(profile);
    request->mutable_browser_report()
        ->mutable_chrome_user_profile_reports(0)
        ->set_policy_data(policy::DictionaryPolicyConversions(std::move(client))
                              .EnablePrettyPrint(false)
                              .ToJSON());

    int64_t timestamp = GetMachineLevelUserCloudPolicyFetchTimestamp();
    if (timestamp > 0) {
      request->mutable_browser_report()
          ->mutable_chrome_user_profile_reports(0)
          ->set_policy_fetched_timestamp(timestamp);
    }
  }
}

bool UpdateJSONEncodedStringEntry(const base::Value& dict_value,
                                  const char key[],
                                  std::string* entry,
                                  const Type type) {
  if (const base::Value* value = dict_value.FindKey(key)) {
    if ((type == DICTIONARY && !value->is_dict()) ||
        (type == LIST && !value->is_list())) {
      return false;
    }
    base::JSONWriter::Write(*value, entry);
  } else {
    if (type == DICTIONARY)
      *entry = kDefaultDictionary;
    else if (type == LIST)
      *entry = kDefaultList;
  }

  return true;
}

void AppendPlatformInformation(em::ChromeDesktopReportRequest* request,
                               const PrefService* prefs) {
  base::Value os_info = base::Value(base::Value::Type::DICTIONARY);
  os_info.SetKey(kOS, base::Value(policy::GetOSPlatform()));
  os_info.SetKey(kOSVersion, base::Value(policy::GetOSVersion()));
  os_info.SetKey(kOSArch, base::Value(policy::GetOSArchitecture()));
  base::JSONWriter::Write(os_info, request->mutable_os_info());

  const char kComputerName[] = "computername";
  base::Value machine_name = base::Value(base::Value::Type::DICTIONARY);
  machine_name.SetKey(kComputerName, base::Value(policy::GetMachineName()));
  base::JSONWriter::Write(machine_name, request->mutable_machine_name());

  const char kUsername[] = "username";
  base::Value os_user = base::Value(base::Value::Type::DICTIONARY);
  os_user.SetKey(kUsername, base::Value(policy::GetOSUsername()));
  base::JSONWriter::Write(os_user, request->mutable_os_user());

#if defined(OS_WIN)
  request->set_serial_number(
      policy::BrowserDMTokenStorage::Get()->RetrieveSerialNumber());
#endif
}

std::unique_ptr<em::ChromeUserProfileReport>
GenerateChromeUserProfileReportRequest(const base::Value& profile_report,
                                       const PrefService* prefs) {
  if (!profile_report.is_dict())
    return nullptr;

  std::unique_ptr<em::ChromeUserProfileReport> request =
      std::make_unique<em::ChromeUserProfileReport>();

  if (!UpdateJSONEncodedStringEntry(profile_report, kChromeSignInUser,
                                    request->mutable_chrome_signed_in_user(),
                                    DICTIONARY)) {
    return nullptr;
  }

  if (prefs->GetBoolean(
          enterprise_reporting::kReportExtensionsAndPluginsData)) {
    if (!UpdateJSONEncodedStringEntry(profile_report, kExtensionData,
                                      request->mutable_extension_data(),
                                      LIST) ||
        !UpdateJSONEncodedStringEntry(profile_report, kPlugins,
                                      request->mutable_plugins(), LIST)) {
      return nullptr;
    }
  }

  if (prefs->GetBoolean(enterprise_reporting::kReportSafeBrowsingData)) {
    if (const base::Value* count =
            profile_report.FindKey(kSafeBrowsingWarnings)) {
      if (!count->is_int())
        return nullptr;
      request->set_safe_browsing_warnings(count->GetInt());
    }

    if (const base::Value* count =
            profile_report.FindKey(kSafeBrowsingWarningsClickThrough)) {
      if (!count->is_int())
        return nullptr;
      request->set_safe_browsing_warnings_click_through(count->GetInt());
    }
  }

  return request;
}

#if defined(OS_WIN)
const wchar_t kDefaultRegistryPath[] =
    L"SOFTWARE\\Google\\Endpoint Verification";
const wchar_t kValueName[] = L"Safe Storage";

std::string ReadEncryptedSecret() {
  base::win::RegKey key;
  DWORD kMaxRawSize = 1024;
  char raw_data[kMaxRawSize];
  DWORD raw_data_size = kMaxRawSize;
  DWORD raw_type;
  if (ERROR_SUCCESS !=
          key.Open(HKEY_CURRENT_USER, kDefaultRegistryPath, KEY_READ) ||
      ERROR_SUCCESS !=
          key.ReadValue(kValueName, raw_data, &raw_data_size, &raw_type) ||
      raw_type != REG_BINARY) {
    return std::string();
  }
  std::string encrypted_secret;
  encrypted_secret.insert(0, raw_data, raw_data_size);
  return encrypted_secret;
}

// Encrypts the |plaintext| and write the result in |cyphertext|. This
// function was taken from os_crypt/os_crypt_win.cc (Chromium).
bool EncryptString(const std::string& plaintext, std::string* ciphertext) {
  DATA_BLOB input;
  input.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plaintext.data()));
  input.cbData = static_cast<DWORD>(plaintext.length());

  DATA_BLOB output;
  BOOL result = ::CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr,
                                   0, &output);
  if (!result)
    return false;

  // this does a copy
  ciphertext->assign(reinterpret_cast<std::string::value_type*>(output.pbData),
                     output.cbData);

  LocalFree(output.pbData);
  return true;
}

// Decrypts the |cyphertext| and write the result in |plaintext|. This
// function was taken from os_crypt/os_crypt_win.cc (Chromium).
bool DecryptString(const std::string& ciphertext, std::string* plaintext) {
  DATA_BLOB input;
  input.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(ciphertext.data()));
  input.cbData = static_cast<DWORD>(ciphertext.length());

  DATA_BLOB output;
  BOOL result =
      ::CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0,
                           &output);
  if (!result)
    return false;

  plaintext->assign(reinterpret_cast<char*>(output.pbData), output.cbData);
  LocalFree(output.pbData);
  return true;
}

std::string CreateRandomSecret() {
  // Generate a password with 128 bits of randomness.
  const int kBytes = 128 / 8;
  std::string secret;
  base::Base64Encode(base::RandBytesAsString(kBytes), &secret);

  std::string encrypted_secret;
  if (!EncryptString(secret, &encrypted_secret)) {
    return std::string();
  }

  base::win::RegKey key;
  if (ERROR_SUCCESS !=
      key.Create(HKEY_CURRENT_USER, kDefaultRegistryPath, KEY_WRITE)) {
    return std::string();
  }
  if (ERROR_SUCCESS != key.WriteValue(kValueName, encrypted_secret.data(),
                                      encrypted_secret.size(), REG_BINARY)) {
    return std::string();
  }
  return secret;
}

#elif defined(OS_MACOSX)  // defined(OS_WIN)

constexpr char kServiceName[] = "Endpoint Verification Safe Storage";
constexpr char kAccountName[] = "Endpoint Verification";

std::string AddRandomPasswordToKeychain(const crypto::AppleKeychain& keychain) {
  // Generate a password with 128 bits of randomness.
  const int kBytes = 128 / 8;
  std::string password;
  base::Base64Encode(base::RandBytesAsString(kBytes), &password);
  void* password_data =
      const_cast<void*>(static_cast<const void*>(password.data()));

  OSStatus error = keychain.AddGenericPassword(
      strlen(kServiceName), kServiceName, strlen(kAccountName), kAccountName,
      password.size(), password_data, nullptr);

  if (error != noErr)
    return std::string();
  return password;
}

std::string ReadEncryptedSecret() {
  UInt32 password_length = 0;
  void* password_data = nullptr;
  crypto::AppleKeychain keychain;
  OSStatus error = keychain.FindGenericPassword(
      strlen(kServiceName), kServiceName, strlen(kAccountName), kAccountName,
      &password_length, &password_data, nullptr);

  if (error == noErr) {
    std::string password =
        std::string(static_cast<char*>(password_data), password_length);
    keychain.ItemFreeContent(password_data);
    return password;
  }

  if (error == errSecItemNotFound) {
    std::string password = AddRandomPasswordToKeychain(keychain);
    return password;
  }

  return std::string();
}

#endif  // defined(OS_MACOSX)

base::FilePath* GetEndpointVerificationDirOverride() {
  static base::NoDestructor<base::FilePath> dir_override;
  return dir_override.get();
}

// Returns "AppData\Local\Google\Endpoint Verification".
base::FilePath GetEndpointVerificationDir() {
  base::FilePath path;
  if (!GetEndpointVerificationDirOverride()->empty())
    return *GetEndpointVerificationDirOverride();
#if defined(OS_WIN)
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path))
#elif defined(OS_LINUX)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  path = base::nix::GetXDGDirectory(env.get(), base::nix::kXdgConfigHomeEnvVar,
                                    base::nix::kDotConfigDir);
  if (path.empty())
#elif defined(OS_MACOSX)
  if (!base::PathService::Get(base::DIR_APP_DATA, &path))
#else
  if (true)
#endif
    return path;
#if defined(OS_LINUX)
  path = path.AppendASCII("google");
#else
  path = path.AppendASCII("Google");
#endif
  path = path.AppendASCII("Endpoint Verification");
  return path;
}

}  // namespace

std::unique_ptr<em::ChromeDesktopReportRequest>
GenerateChromeDesktopReportRequest(const base::DictionaryValue& report,
                                   Profile* profile) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      std::make_unique<em::ChromeDesktopReportRequest>();

  const PrefService* prefs = profile->GetPrefs();

  AppendPlatformInformation(request.get(), prefs);

  if (const base::Value* browser_report =
          report.FindKeyOfType(kBrowserReport, base::Value::Type::DICTIONARY)) {
    if (const base::Value* profile_reports = browser_report->FindKeyOfType(
            kChromeUserProfileReport, base::Value::Type::LIST)) {
      if (!profile_reports->GetList().empty()) {
        DCHECK_EQ(1u, profile_reports->GetList().size());
        // Currently, profile send their browser reports individually.
        std::unique_ptr<em::ChromeUserProfileReport> profile_report_request =
            GenerateChromeUserProfileReportRequest(
                profile_reports->GetList()[0], prefs);
        if (!profile_report_request)
          return nullptr;
        request->mutable_browser_report()
            ->mutable_chrome_user_profile_reports()
            ->AddAllocated(profile_report_request.release());
      }
    }
  }

  AppendAdditionalBrowserInformation(request.get(), profile);

  return request;
}

// Sets the path used to store Endpoint Verification data for tests.
void OverrideEndpointVerificationDirForTesting(const base::FilePath& path) {
  *GetEndpointVerificationDirOverride() = path;
}

void StoreDeviceData(const std::string& id,
                     const std::unique_ptr<std::vector<uint8_t>> data,
                     base::OnceCallback<void(bool)> callback) {
  base::FilePath data_file = GetEndpointVerificationDir();
  if (data_file.empty()) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(pastarmovj): Make sure the resulting path is still a direct file or
  // subdir+file of the EV folder.
  data_file = data_file.AppendASCII(id);

  bool success = false;
  if (data) {
    // Ensure the directory exists.
    success = base::CreateDirectory(data_file.DirName());
    if (!success) {
      LOG(ERROR) << "Could not create directory: "
                 << data_file.DirName().LossyDisplayName();
      std::move(callback).Run(false);
      return;
    }

    base::FilePath tmp_path;
    success = base::CreateTemporaryFileInDir(data_file.DirName(), &tmp_path);
    if (!success) {
      LOG(ERROR) << "Could not open file for writing: "
                 << tmp_path.LossyDisplayName();
      std::move(callback).Run(false);
      return;
    }

    base::WriteFile(tmp_path, reinterpret_cast<const char*>(data->data()),
                    data->size());
    success = base::Move(tmp_path, data_file);
  } else {
    // Not passing a second parameter means clear the data sored under |id|.
    success = base::DeleteFile(data_file, false);
    if (base::IsDirectoryEmpty(data_file.DirName()))
      base::DeleteFile(data_file.DirName(), false);
  }

  std::move(callback).Run(success);
}

void RetrieveDeviceData(
    const std::string& id,
    base::OnceCallback<void(const std::string&, bool)> callback) {
  base::FilePath data_file = GetEndpointVerificationDir();
  if (data_file.empty()) {
    std::move(callback).Run("", false);
    return;
  }
  data_file = data_file.AppendASCII(id);
  // If the file does not exist don't treat this as an error rather return an
  // empty string.
  if (!base::PathExists(data_file)) {
    std::move(callback).Run("", true);
    return;
  }
  std::string data;
  // ReadFileToString does not permit traversal with .. so this is guaranteed to
  // be a descendant of the data directory up to links created outside of
  // Chrome.
  bool result = base::ReadFileToString(data_file, &data);

  std::move(callback).Run(data, result);
}

void RetrieveDeviceSecret(
    base::OnceCallback<void(const std::string&, bool)> callback) {
  std::string secret;
#if defined(OS_WIN)
  std::string encrypted_secret = ReadEncryptedSecret();
  if (encrypted_secret.empty()) {
    secret = CreateRandomSecret();
    std::move(callback).Run(secret, !secret.empty());
    return;
  }
  if (!DecryptString(encrypted_secret, &secret)) {
    std::move(callback).Run("", false);
    return;
  }
#elif defined(OS_MACOSX)
  secret = ReadEncryptedSecret();
  if (secret.empty()) {
    std::move(callback).Run(secret, false);
    return;
  }

#endif
  std::move(callback).Run(secret, true);
}

}  // namespace extensions
