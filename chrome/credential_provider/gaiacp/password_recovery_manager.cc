// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/password_recovery_manager.h"

#include <windows.h>
#include <winternl.h>

#include <lm.h>  // Needed for LSA_UNICODE_STRING
#include <process.h>

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <ntsecapi.h>  // For POLICY_ALL_ACCESS types

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {

const base::TimeDelta
    PasswordRecoveryManager::kDefaultEscrowServiceRequestTimeout =
        base::TimeDelta::FromMilliseconds(3000);

namespace {

typedef std::vector<std::pair<std::string, std::string*>>
    UrlFetchResultNeedOutputs;

// Constants for storing password recovery information in the LSA.
constexpr char kUserPasswordLsaStoreIdKey[] = "resource_id";
constexpr char kUserPasswordLsaStoreEncryptedPasswordKey[] =
    "encrypted_password";

// Constants used for contacting the password escrow service.
const char kEscrowServiceGenerateKeyPairPath[] = "/v1/generateKeyPair";
const char kGenerateKeyPairRequestDeviceIdParameterName[] = "device_id";
const char kGenerateKeyPairResponsePublicKeyParameterName[] =
    "base64_public_key";
const char kGenerateKeyPairResponseResourceIdParameterName[] = "resource_id";

const char kEscrowServiceGetPrivateKeyPath[] = "/v1/getPrivateKey";
const char kGetPrivateKeyRequestResourceIdParameterName[] = "resource_id";
const char kGetPrivateKeyResponsePrivateKeyParameterName[] =
    "base64_private_key";

constexpr wchar_t kUserPasswordLsaStoreKeyPrefix[] =
#if defined(GOOGLE_CHROME_BUILD)
    L"Chrome-GCPW-";
#else
    L"Chromium-GCPW-";
#endif

// Self deleting escrow service requester. This class will try to make a query
// using the given url fetcher. It will delete itself when the request is
// completed, either because the request completed successfully within the
// timeout or the request has timed out and is allowed to complete in the
// background without having the result read by anyone.
// There are two situations where the request will be deleted:
// 1. If the background thread making the request returns within the given
// timeout, the function is guaranteed to return the result that was fetched.
// 2. If however the background thread times out there are two potential
// race conditions that can occur:
//    1. The main thread making the request can mark that the background thread
//       is orphaned before it can complete. In this case when the background
//       thread completes it will check whether the request is orphaned and self
//       delete.
//    2. The background thread completes before the main thread can mark the
//       request as orphaned. In this case the background thread will have
//       marked that the request is no longer processing and thus the main
//       thread can self delete.
class EscrowServiceRequest {
 public:
  explicit EscrowServiceRequest(std::unique_ptr<WinHttpUrlFetcher> fetcher)
      : fetcher_(std::move(fetcher)) {
    DCHECK(fetcher_);
  }

  // Tries to fetch the request stored in |fetcher_| in a background thread
  // within the given |request_timeout|. If the background thread returns before
  // the timeout expires, it is guaranteed that a result can be returned and the
  // requester will delete itself.
  base::Optional<base::Value> WaitForResponseFromEscrowService(
      const base::TimeDelta& request_timeout) {
    base::Optional<base::Value> result;

    // Start the thread and wait on its handle until |request_timeout| expires
    // or the thread finishes.
    unsigned wait_thread_id;
    uintptr_t wait_thread = ::_beginthreadex(
        nullptr, 0, &EscrowServiceRequest::FetchResultFromEscrowService,
        reinterpret_cast<void*>(this), 0, &wait_thread_id);

    HRESULT hr = S_OK;
    if (wait_thread == 0) {
      return result;
    } else {
      // Hold the handle in the scoped handle so that it can be immediately
      // closed when the wait is complete allowing the thread to finish
      // completely if needed.
      base::win::ScopedHandle thread_handle(
          reinterpret_cast<HANDLE>(wait_thread));
      hr = ::WaitForSingleObject(thread_handle.Get(),
                                 request_timeout.InMilliseconds());
    }

    // The race condition starts here. It is possible that between the expiry of
    // the timeout in the call for WaitForSingleObject and the call to
    // OrphanRequest, the fetching thread could have finished. So there is a two
    // part handshake. Either the background thread has called ProcessingDone
    // in which case it has already passed its own check for |is_orphaned_| and
    // the call to OrphanRequest should delete this object right now. Otherwise
    // the background thread is still running and will be able to query the
    // |is_orphaned_| state and delete the object after thread completion.
    if (hr != WAIT_OBJECT_0) {
      LOGFN(ERROR) << "Wait for response timed out or failed hr=" << putHR(hr);
      OrphanRequest();
      return result;
    }

    result = base::JSONReader::Read(
        base::StringPiece(response_.data(), response_.size()),
        base::JSON_ALLOW_TRAILING_COMMAS);

    if (!result || !result->is_dict()) {
      LOGFN(ERROR) << "Failed to read json result from server response";
      result.reset();
    }

    delete this;
    return result;
  }

 private:
  void OrphanRequest() {
    bool delete_self = false;
    {
      base::AutoLock locker(orphan_lock_);
      CHECK(!is_orphaned_);
      if (!is_processing_) {
        delete_self = true;
      } else {
        is_orphaned_ = true;
      }
    }

    if (delete_self)
      delete this;
  }

  void ProcessingDone() {
    bool delete_self = false;
    {
      base::AutoLock locker(orphan_lock_);
      CHECK(is_processing_);
      if (is_orphaned_) {
        delete_self = true;
      } else {
        is_processing_ = false;
      }
    }

    if (delete_self)
      delete this;
  }

  // Background thread function that is used to query the request to the
  // escrow service. This thread never times out and simply marks the fetcher
  // as finished processing when it is done.
  static unsigned __stdcall FetchResultFromEscrowService(void* param) {
    DCHECK(param);
    EscrowServiceRequest* requester =
        reinterpret_cast<EscrowServiceRequest*>(param);

    HRESULT hr = requester->fetcher_->Fetch(&requester->response_);
    if (FAILED(hr))
      LOGFN(INFO) << "fetcher.Fetch hr=" << putHR(hr);

    requester->ProcessingDone();
    return 0;
  }

  base::Lock orphan_lock_;
  std::unique_ptr<WinHttpUrlFetcher> fetcher_;
  std::vector<char> response_;
  bool is_orphaned_ = false;
  bool is_processing_ = true;
};

// Builds the required json request to be sent to the escrow service and fetches
// the json response from the escrow service (if any). Returns S_OK if
// |needed_outputs| can be filled correctly with the requested data, otherwise
// returns an error code.
// |request_url| is the full query url from which to fetch a response.
// |headers| are all the header key value pairs to be sent with the request.
// |parameters| are all the json parameters to be sent with the request. This
// argument will be converted to a json string and sent as part of the body of
// the request.
// |request_timeout| is the maximum time to wait for a response.
// |needed_outputs| is the mapping of the desired result key to an address where
// the result can be stored.
// If any |needed_outputs| is missing, all of the outputs are cleared.
HRESULT BuildRequestAndFetchResultFromEscrowService(
    const GURL& request_url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::vector<std::pair<std::string, std::string>>& parameters,
    const UrlFetchResultNeedOutputs& needed_outputs,
    const base::TimeDelta& request_timeout) {
  DCHECK(needed_outputs.size());

  if (request_url.is_empty()) {
    LOGFN(ERROR) << "No escrow service url specified";
    return E_FAIL;
  }

  auto url_fetcher = WinHttpUrlFetcher::Create(request_url);
  if (!url_fetcher) {
    LOGFN(ERROR) << "Could not create valid fetcher for url="
                 << request_url.spec();
    return E_FAIL;
  }

  url_fetcher->SetRequestHeader("Content-Type", "application/json");

  for (auto& header : headers)
    url_fetcher->SetRequestHeader(header.first.c_str(), header.second.c_str());

  base::Value request_dict(base::Value::Type::DICTIONARY);

  for (auto& parameter : parameters)
    request_dict.SetStringKey(parameter.first, parameter.second);

  std::string json;
  if (!base::JSONWriter::Write(request_dict, &json)) {
    LOGFN(ERROR) << "base::JSONWriter::Write failed";
    return E_FAIL;
  }

  HRESULT hr = url_fetcher->SetRequestBody(json.c_str());
  if (FAILED(hr)) {
    LOGFN(ERROR) << "fetcher.SetRequestBody hr=" << putHR(hr);
    return E_FAIL;
  }

  base::Optional<base::Value> request_result =
      (new EscrowServiceRequest(std::move(url_fetcher)))
          ->WaitForResponseFromEscrowService(request_timeout);

  if (!request_result)
    return E_FAIL;

  for (const std::pair<std::string, std::string*>& output : needed_outputs) {
    const std::string* output_value =
        request_result->FindStringKey(output.first);
    if (!output_value) {
      LOGFN(ERROR) << "Could not extract value '" << output.first
                   << "' from server response";
      hr = E_FAIL;
      break;
    }
    DCHECK(output.second);
    *output.second = *output_value;
  }

  if (FAILED(hr)) {
    for (const std::pair<std::string, std::string*>& output : needed_outputs)
      output.second->clear();
  }

  return hr;
}

// Makes a standard: "Authorization: Bearer $TOKEN" header for passing
// authorization information to a server.
std::pair<std::string, std::string> MakeAuthorizationHeader(
    const std::string& access_token) {
  return {"Authorization", "Bearer " + access_token};
}

// Request a new public key and corresponding resource id from the escrow
// service in order to encrypt |password|. |access_token| is used to authorize
// the request on the escrow service. |device_id| is used to identify the device
// making the request. Fills in |encrypted_data| the resource id for the
// encryption key and also with the encryped password.
HRESULT EncryptUserPasswordUsingEscrowService(
    const std::string& access_token,
    const std::string& device_id,
    const base::string16& password,
    const base::TimeDelta& request_timeout,
    base::Optional<base::Value>* encrypted_data) {
  DCHECK(encrypted_data);
  DCHECK(!(*encrypted_data));

  std::string resource_id;
  std::string public_key;

  // Fetch the results and extract the |resource_id| for the key and the
  // |public_key| to be used for encryption.
  HRESULT hr = BuildRequestAndFetchResultFromEscrowService(
      PasswordRecoveryManager::Get()->GetEscrowServiceGenerateKeyPairUrl(),
      {MakeAuthorizationHeader(access_token)},
      {{kGenerateKeyPairRequestDeviceIdParameterName, device_id}},
      {
          {kGenerateKeyPairResponseResourceIdParameterName, &resource_id},
          {kGenerateKeyPairResponsePublicKeyParameterName, &public_key},
      },
      request_timeout);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromEscrowService hr="
                 << putHR(hr);
    return E_FAIL;
  }

  encrypted_data->emplace(base::Value(base::Value::Type::DICTIONARY));
  (*encrypted_data)->SetStringKey(kUserPasswordLsaStoreIdKey, resource_id);

  (*encrypted_data)
      ->SetStringKey(kUserPasswordLsaStoreEncryptedPasswordKey,
                     base::UTF16ToUTF8(password));

  return hr;
}

// Given the |encrypted_data| which would contain the resource id of the
// encryption key and the encrypted password, recovers the |decrypted_password|
// by getting the private key from the escrow service and decrypting the
// password. |access_token| is used to authorize the request on the escrow
// service.
HRESULT DecryptUserPasswordUsingEscrowService(
    const std::string& access_token,
    const base::Optional<base::Value>& encrypted_data,
    const base::TimeDelta& request_timeout,
    base::string16* decrypted_password) {
  if (!encrypted_data)
    return E_FAIL;
  DCHECK(decrypted_password);
  DCHECK(encrypted_data && encrypted_data->is_dict());
  const std::string* resource_id =
      encrypted_data->FindStringKey(kUserPasswordLsaStoreIdKey);
  const std::string* encrypted_password =
      encrypted_data->FindStringKey(kUserPasswordLsaStoreEncryptedPasswordKey);

  if (!resource_id) {
    LOGFN(ERROR) << "No password resource id found to restore";
    return E_FAIL;
  }

  if (!encrypted_password) {
    LOGFN(ERROR) << "No encrypted password found to restore";
    return E_FAIL;
  }

  std::string private_key;

  // Fetch the results and extract the |private_key| to be used for decryption.
  HRESULT hr = BuildRequestAndFetchResultFromEscrowService(
      PasswordRecoveryManager::Get()->GetEscrowServiceGetPrivateKeyUrl(),
      {MakeAuthorizationHeader(access_token)},
      {{kGetPrivateKeyRequestResourceIdParameterName, *resource_id}},
      {
          {kGetPrivateKeyResponsePrivateKeyParameterName, &private_key},
      },
      request_timeout);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromEscrowService hr="
                 << putHR(hr);
    return E_FAIL;
  }
  // Move semantics should ensure the temporary password is directly moved
  // into |decrypted_password| and thus does not need to be securely zeroed.
  *decrypted_password = base::UTF8ToUTF16(*encrypted_password);

  return S_OK;
}

}  // namespace

// static
PasswordRecoveryManager* PasswordRecoveryManager::Get() {
  return *GetInstanceStorage();
}

// static
PasswordRecoveryManager** PasswordRecoveryManager::GetInstanceStorage() {
  static PasswordRecoveryManager instance(kDefaultEscrowServiceRequestTimeout);
  static PasswordRecoveryManager* instance_storage = &instance;

  return &instance_storage;
}

PasswordRecoveryManager::PasswordRecoveryManager(
    base::TimeDelta request_timeout)
    : request_timeout_(request_timeout) {}

PasswordRecoveryManager::~PasswordRecoveryManager() = default;

HRESULT PasswordRecoveryManager::GetUserPasswordLsaStoreKey(
    const base::string16& sid,
    base::string16* store_key) {
  DCHECK(store_key);
  DCHECK(sid.size());

  *store_key = kUserPasswordLsaStoreKeyPrefix + sid;
  return S_OK;
}

HRESULT PasswordRecoveryManager::ClearUserRecoveryPassword(
    const base::string16& sid) {
  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  if (!policy) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }
  base::string16 store_key;
  HRESULT hr = GetUserPasswordLsaStoreKey(sid, &store_key);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetUserPasswordLsaStoreKey hr=" << putHR(hr);
    return hr;
  }
  return policy->RemovePrivateData(store_key.c_str());
}

HRESULT PasswordRecoveryManager::StoreWindowsPasswordIfNeeded(
    const base::string16& sid,
    const std::string& access_token,
    const base::string16& password) {
  if (!MdmPasswordRecoveryEnabled())
    return E_NOTIMPL;

  base::string16 machine_guid;
  HRESULT hr = GetMachineGuid(&machine_guid);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "Failed to get machine GUID hr=" << putHR(hr);
    return hr;
  }

  std::string device_id = base::UTF16ToUTF8(machine_guid);

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  if (!policy) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  // See if a password key is already stored in the LSA for this user.
  base::string16 store_key;
  hr = GetUserPasswordLsaStoreKey(sid, &store_key);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetUserPasswordLsaStoreKey hr=" << putHR(hr);
    return hr;
  }

  // Only check if a value already exists for the user's password. The call to
  // RetrievePrivateData always succeeds if the value exists, regardless of
  // the size of the buffer passed in. It will merely copy whatever it can
  // into the buffer. In this case we don't care about the contents and
  // just want to check the existence of a value.
  wchar_t password_lsa_data[32];
  hr = policy->RetrievePrivateData(store_key.c_str(), password_lsa_data,
                                   base::size(password_lsa_data));
  if (SUCCEEDED(hr)) {
    SecurelyClearBuffer(password_lsa_data, sizeof(password_lsa_data));
    return S_OK;
  }

  base::Optional<base::Value> encrypted_dict;
  hr = EncryptUserPasswordUsingEscrowService(access_token, device_id, password,
                                             request_timeout_, &encrypted_dict);
  if (SUCCEEDED(hr)) {
    std::string lsa_value;
    if (base::JSONWriter::Write(encrypted_dict.value(), &lsa_value)) {
      base::string16 lsa_value16 = base::UTF8ToUTF16(lsa_value);
      hr = policy->StorePrivateData(store_key.c_str(), lsa_value16.c_str());
      SecurelyClearString(lsa_value16);
      SecurelyClearString(lsa_value);

      if (FAILED(hr)) {
        LOGFN(ERROR) << "StorePrivateData hr=" << putHR(hr);
        return hr;
      }
    } else {
      LOGFN(ERROR) << "base::JSONWriter::Write failed";
      return E_FAIL;
    }

    SecurelyClearDictionaryValueWithKey(
        &encrypted_dict, kUserPasswordLsaStoreEncryptedPasswordKey);
  } else {
    LOGFN(ERROR) << "EncryptUserPasswordUsingEscrowService hr=" << putHR(hr);
    return E_FAIL;
  }
  return S_OK;
}

HRESULT PasswordRecoveryManager::RecoverWindowsPasswordIfPossible(
    const base::string16& sid,
    const std::string& access_token,
    base::string16* recovered_password) {
  if (!MdmPasswordRecoveryEnabled())
    return E_NOTIMPL;

  DCHECK(recovered_password);

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  if (!policy) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  // See if a password key is already stored in the LSA for this user.
  base::string16 store_key;
  HRESULT hr = GetUserPasswordLsaStoreKey(sid, &store_key);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetUserPasswordLsaStoreKey hr=" << putHR(hr);
    return hr;
  }

  wchar_t password_lsa_data[1024];
  hr = policy->RetrievePrivateData(store_key.c_str(), password_lsa_data,
                                   base::size(password_lsa_data));

  if (FAILED(hr))
    LOGFN(ERROR) << "RetrievePrivateData hr=" << putHR(hr);

  std::string json_string = base::UTF16ToUTF8(password_lsa_data);
  base::Optional<base::Value> encrypted_dict =
      base::JSONReader::Read(json_string, base::JSON_ALLOW_TRAILING_COMMAS);
  SecurelyClearString(json_string);
  SecurelyClearBuffer(password_lsa_data, sizeof(password_lsa_data));

  base::string16 decrypted_password;
  hr = DecryptUserPasswordUsingEscrowService(
      access_token, encrypted_dict, request_timeout_, &decrypted_password);

  if (encrypted_dict) {
    SecurelyClearDictionaryValueWithKey(
        &encrypted_dict, kUserPasswordLsaStoreEncryptedPasswordKey);
  }

  if (SUCCEEDED(hr))
    *recovered_password = decrypted_password;
  SecurelyClearString(decrypted_password);

  return hr;
}

GURL PasswordRecoveryManager::GetEscrowServiceGenerateKeyPairUrl() {
  if (!MdmPasswordRecoveryEnabled())
    return GURL();

  GURL escrow_service_server = MdmEscrowServiceUrl();

  if (escrow_service_server.is_empty()) {
    LOGFN(ERROR) << "No escrow service server specified";
    return GURL();
  }

  return escrow_service_server.Resolve(kEscrowServiceGenerateKeyPairPath);
}

GURL PasswordRecoveryManager::GetEscrowServiceGetPrivateKeyUrl() {
  if (!MdmPasswordRecoveryEnabled())
    return GURL();

  GURL escrow_service_server = MdmEscrowServiceUrl();

  if (escrow_service_server.is_empty()) {
    LOGFN(ERROR) << "No escrow service server specified";
    return GURL();
  }

  return escrow_service_server.Resolve(kEscrowServiceGetPrivateKeyPath);
}

std::string PasswordRecoveryManager::MakeGenerateKeyPairResponseForTesting(
    const std::string& public_key,
    const std::string& resource_id) {
  return base::StringPrintf(
      R"({"%s": "%s", "%s": "%s"})",
      kGenerateKeyPairResponsePublicKeyParameterName, public_key.c_str(),
      kGenerateKeyPairResponseResourceIdParameterName, resource_id.c_str());
}

std::string PasswordRecoveryManager::MakeGetPrivateKeyResponseForTesting(
    const std::string& private_key) {
  return base::StringPrintf(R"({"%s": "%s"})",
                            kGetPrivateKeyResponsePrivateKeyParameterName,
                            private_key.c_str());
}

}  // namespace credential_provider
