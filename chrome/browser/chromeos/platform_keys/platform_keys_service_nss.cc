// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"

#include <cert.h>
#include <certdb.h>
#include <cryptohi.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <pkcs11t.h>
#include <secder.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/net/client_cert_store_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system_token_cert_db_initializer.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "crypto/nss_key_util.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/third_party/mozilla_security_manager/nsNSSCertificateDB.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/cros_system_api/constants/pkcs11_custom_attributes.h"

using content::BrowserContext;
using content::BrowserThread;

namespace {
const char kErrorInternal[] = "Internal Error.";
const char kErrorKeyNotFound[] = "Key not found.";
const char kErrorCertificateNotFound[] = "Certificate could not be found.";
const char kErrorAlgorithmNotSupported[] = "Algorithm not supported.";

// The current maximal RSA modulus length that ChromeOS's TPM supports for key
// generation.
const unsigned int kMaxRSAModulusLengthBits = 2048;
}  // namespace

namespace chromeos {
namespace platform_keys {

namespace {

using ServiceWeakPtr = base::WeakPtr<PlatformKeysServiceImpl>;

// Base class to store state that is common to all NSS database operations and
// to provide convenience methods to call back.
// Keeps track of the originating task runner.
class NSSOperationState {
 public:
  explicit NSSOperationState(ServiceWeakPtr weak_ptr);
  virtual ~NSSOperationState() = default;

  // Called if an error occurred during the execution of the NSS operation
  // described by this object.
  virtual void OnError(const base::Location& from,
                       const std::string& error_message) = 0;

  static void RunCallback(base::OnceClosure callback, ServiceWeakPtr weak_ptr) {
    if (weak_ptr) {
      std::move(callback).Run();
    }
  }

  crypto::ScopedPK11Slot slot_;

  // Weak pointer to the PlatformKeysServiceImpl that created this state. Used
  // to check if the callback should be still called.
  ServiceWeakPtr service_weak_ptr_;
  // The task runner on which the NSS operation was called. Any reply must be
  // posted to this runner.
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NSSOperationState);
};

using GetCertDBCallback = base::Callback<void(net::NSSCertDatabase* cert_db)>;

// Used by GetCertDatabaseOnIoThread and called back with the requested
// NSSCertDatabase.
// If |token_id| is not empty, sets |slot_| of |state| accordingly and calls
// |callback| if the database was successfully retrieved.
void DidGetCertDbOnIoThread(const std::string& token_id,
                            const GetCertDBCallback& callback,
                            NSSOperationState* state,
                            net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!cert_db) {
    LOG(ERROR) << "Couldn't get NSSCertDatabase.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  if (!token_id.empty()) {
    if (token_id == kTokenIdUser)
      state->slot_ = cert_db->GetPrivateSlot();
    else if (token_id == kTokenIdSystem)
      state->slot_ = cert_db->GetSystemSlot();

    if (!state->slot_) {
      LOG(ERROR) << "Slot for token id '" << token_id << "' not available.";
      state->OnError(FROM_HERE, kErrorInternal);
      return;
    }
  }

  callback.Run(cert_db);
}

// Retrieves the NSSCertDatabase from |context| and, if |token_id| is not empty,
// the slot for |token_id|.
// Must be called on the IO thread.
void GetCertDatabaseOnIoThread(const std::string& token_id,
                               const GetCertDBCallback& callback,
                               content::ResourceContext* context,
                               NSSOperationState* state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::NSSCertDatabase* cert_db = GetNSSCertDatabaseForResourceContext(
      context, base::Bind(&DidGetCertDbOnIoThread, token_id, callback, state));

  if (cert_db)
    DidGetCertDbOnIoThread(token_id, callback, state, cert_db);
}

// Called by SystemTokenCertDBInitializer on the UI thread with the system token
// certificate database when it is initialized.
void DidGetSystemTokenCertDbOnUiThread(const std::string& token_id,
                                       const GetCertDBCallback& callback,
                                       NSSOperationState* state,
                                       net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Sets |slot_| of |state| accordingly and calls |callback| on the IO thread
  // if the database was successfully retrieved.
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&DidGetCertDbOnIoThread, token_id, callback,
                                state, cert_db));
}

// Asynchronously fetches the NSSCertDatabase for |browser_context| and, if
// |token_id| is not empty, the slot for |token_id|. Stores the slot in |state|
// and passes the database to |callback|. Will run |callback| on the IO thread.
// TODO(omorsi): Introduce timeout for retrieving certificate database in
// platform keys.
void GetCertDatabase(const std::string& token_id,
                     const GetCertDBCallback& callback,
                     BrowserContext* browser_context,
                     NSSOperationState* state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  // There will be no public or private slots initialized if no user is logged
  // in. In this case, an NSS certificate database that has the system slot
  // should be used for system token operations.
  if (ProfileHelper::IsSigninProfile(profile)) {
    SystemTokenCertDBInitializer::Get()->GetSystemTokenCertDb(base::BindOnce(
        &DidGetSystemTokenCertDbOnUiThread, token_id, callback, state));
    return;
  }

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&GetCertDatabaseOnIoThread, token_id, callback,
                                browser_context->GetResourceContext(), state));
}

class GenerateRSAKeyState : public NSSOperationState {
 public:
  GenerateRSAKeyState(ServiceWeakPtr weak_ptr,
                      unsigned int modulus_length_bits,
                      const GenerateKeyCallback& callback);
  ~GenerateRSAKeyState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, std::string() /* no public key */, error_message);
  }

  void CallBack(const base::Location& from,
                const std::string& public_key_spki_der,
                const std::string& error_message) {
    auto bound_callback =
        base::BindOnce(callback_, public_key_spki_der, error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  const unsigned int modulus_length_bits_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  GenerateKeyCallback callback_;
};

class GenerateECKeyState : public NSSOperationState {
 public:
  GenerateECKeyState(ServiceWeakPtr weak_ptr,
                     const std::string& named_curve,
                     const GenerateKeyCallback& callback);
  ~GenerateECKeyState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, std::string() /* no public key */, error_message);
  }

  void CallBack(const base::Location& from,
                const std::string& public_key_spki_der,
                const std::string& error_message) {
    auto bound_callback =
        base::BindOnce(callback_, public_key_spki_der, error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  const std::string named_curve_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  GenerateKeyCallback callback_;
};

class SignState : public NSSOperationState {
 public:
  SignState(ServiceWeakPtr weak_ptr,
            const std::string& data,
            const std::string& public_key_spki_der,
            bool raw_pkcs1,
            HashAlgorithm hash_algorithm,
            const KeyType key_type,
            const SignCallback& callback);
  ~SignState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, std::string() /* no signature */, error_message);
  }

  void CallBack(const base::Location& from,
                const std::string& signature,
                const std::string& error_message) {
    auto bound_callback = base::BindOnce(callback_, signature, error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // The data that will be signed.
  const std::string data_;

  // Must be the DER encoding of a SubjectPublicKeyInfo.
  const std::string public_key_spki_der_;

  // If true, |data_| will not be hashed before signing. Only PKCS#1 v1.5
  // padding will be applied before signing.
  // If false, |hash_algorithm_| is set to a value != NONE.
  const bool raw_pkcs1_;

  // Determines the hash algorithm that is used to digest |data| before signing.
  // Ignored if |raw_pkcs1_| is true.
  const HashAlgorithm hash_algorithm_;

  // Determines the type of the key that should be used for signing. This is
  // specified by the state creator.
  // Note: This can be different from the type of |public_key_spki_der|. In such
  // case, a runtime error should be thrown.
  const KeyType key_type_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  SignCallback callback_;
};

class SelectCertificatesState : public NSSOperationState {
 public:
  explicit SelectCertificatesState(
      ServiceWeakPtr weak_ptr,
      const scoped_refptr<net::SSLCertRequestInfo>& request,
      const SelectCertificatesCallback& callback);
  ~SelectCertificatesState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, std::unique_ptr<net::CertificateList>() /* no matches */,
             error_message);
  }

  void CallBack(const base::Location& from,
                std::unique_ptr<net::CertificateList> matches,
                const std::string& error_message) {
    auto bound_callback =
        base::BindOnce(callback_, std::move(matches), error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  std::unique_ptr<net::ClientCertStore> cert_store_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  SelectCertificatesCallback callback_;
};

class GetCertificatesState : public NSSOperationState {
 public:
  explicit GetCertificatesState(ServiceWeakPtr weak_ptr,
                                const GetCertificatesCallback& callback);
  ~GetCertificatesState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from,
             std::unique_ptr<net::CertificateList>() /* no certificates */,
             error_message);
  }

  void CallBack(const base::Location& from,
                std::unique_ptr<net::CertificateList> certs,
                const std::string& error_message) {
    auto bound_callback =
        base::BindOnce(callback_, std::move(certs), error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  net::ScopedCERTCertificateList certs_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  GetCertificatesCallback callback_;
};

class GetAllKeysState : public NSSOperationState {
 public:
  explicit GetAllKeysState(ServiceWeakPtr weak_ptr,
                           GetAllKeysCallback callback);
  ~GetAllKeysState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, std::vector<std::string>() /* no public keys */,
             error_message);
  }

  void CallBack(const base::Location& from,
                std::vector<std::string> public_key_spki_der_list,
                const std::string& error_message) {
    auto bound_callback =
        base::BindOnce(std::move(callback_),
                       std::move(public_key_spki_der_list), error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

 private:
  // Must be called on origin thread, therefore use CallBack().
  GetAllKeysCallback callback_;
};

class ImportCertificateState : public NSSOperationState {
 public:
  ImportCertificateState(ServiceWeakPtr weak_ptr,
                         const scoped_refptr<net::X509Certificate>& certificate,
                         const ImportCertificateCallback& callback);
  ~ImportCertificateState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, error_message);
  }

  void CallBack(const base::Location& from, const std::string& error_message) {
    auto bound_callback = base::BindOnce(callback_, error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  scoped_refptr<net::X509Certificate> certificate_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  ImportCertificateCallback callback_;
};

class RemoveCertificateState : public NSSOperationState {
 public:
  RemoveCertificateState(ServiceWeakPtr weak_ptr,
                         const scoped_refptr<net::X509Certificate>& certificate,
                         const RemoveCertificateCallback& callback);
  ~RemoveCertificateState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, error_message);
  }

  void CallBack(const base::Location& from, const std::string& error_message) {
    auto bound_callback = base::BindOnce(callback_, error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  scoped_refptr<net::X509Certificate> certificate_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  RemoveCertificateCallback callback_;
};

class RemoveKeyState : public NSSOperationState {
 public:
  RemoveKeyState(ServiceWeakPtr weak_ptr,
                 const std::string& public_key_spki_der,
                 RemoveKeyCallback callback);
  ~RemoveKeyState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, error_message);
  }

  void CallBack(const base::Location& from, const std::string& error_message) {
    auto bound_callback = base::BindOnce(std::move(callback_), error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be a DER encoding of a SubjectPublicKeyInfo.
  const std::string public_key_spki_der_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  RemoveKeyCallback callback_;
};

class GetTokensState : public NSSOperationState {
 public:
  explicit GetTokensState(ServiceWeakPtr weak_ptr,
                          const GetTokensCallback& callback);
  ~GetTokensState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from,
             std::unique_ptr<std::vector<std::string>>() /* no token ids */,
             error_message);
  }

  void CallBack(const base::Location& from,
                std::unique_ptr<std::vector<std::string>> token_ids,
                const std::string& error_message) {
    auto bound_callback =
        base::BindOnce(callback_, std::move(token_ids), error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

 private:
  // Must be called on origin thread, therefore use CallBack().
  GetTokensCallback callback_;
};

class GetKeyLocationsState : public NSSOperationState {
 public:
  GetKeyLocationsState(ServiceWeakPtr weak_ptr,
                       const std::string& public_key_spki_der,
                       const GetKeyLocationsCallback& callback);
  ~GetKeyLocationsState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, std::vector<std::string>(), error_message);
  }

  void CallBack(const base::Location& from,
                const std::vector<std::string>& token_ids,
                const std::string& error_message) {
    auto bound_callback = base::BindOnce(callback_, token_ids, error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be a DER encoding of a SubjectPublicKeyInfo.
  const std::string public_key_spki_der_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  GetKeyLocationsCallback callback_;
};

class SetAttributeForKeyState : public NSSOperationState {
 public:
  SetAttributeForKeyState(ServiceWeakPtr weak_ptr,
                          const std::string& public_key_spki_der,
                          CK_ATTRIBUTE_TYPE attribute_type,
                          const std::string& attribute_value,
                          SetAttributeForKeyCallback callback);
  ~SetAttributeForKeyState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, error_message);
  }

  void CallBack(const base::Location& from, const std::string& error_message) {
    auto bound_callback = base::BindOnce(std::move(callback_), error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be a DER encoding of a SubjectPublicKeyInfo.
  const std::string public_key_spki_der_;
  const CK_ATTRIBUTE_TYPE attribute_type_;
  const std::string attribute_value_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  SetAttributeForKeyCallback callback_;
};

class GetAttributeForKeyState : public NSSOperationState {
 public:
  GetAttributeForKeyState(ServiceWeakPtr weak_ptr,
                          const std::string& public_key_spki_der,
                          CK_ATTRIBUTE_TYPE attribute_type,
                          GetAttributeForKeyCallback callback);
  ~GetAttributeForKeyState() override = default;

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, std::string() /* no attribute value */, error_message);
  }

  void CallBack(const base::Location& from,
                const std::string& attribute_value,
                const std::string& error_message) {
    auto bound_callback =
        base::BindOnce(std::move(callback_), attribute_value, error_message);
    origin_task_runner_->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be a DER encoding of a SubjectPublicKeyInfo.
  const std::string public_key_spki_der_;
  const CK_ATTRIBUTE_TYPE attribute_type_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  GetAttributeForKeyCallback callback_;
};

NSSOperationState::NSSOperationState(ServiceWeakPtr weak_ptr)
    : service_weak_ptr_(weak_ptr),
      origin_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

GenerateRSAKeyState::GenerateRSAKeyState(ServiceWeakPtr weak_ptr,
                                         unsigned int modulus_length_bits,
                                         const GenerateKeyCallback& callback)
    : NSSOperationState(weak_ptr),
      modulus_length_bits_(modulus_length_bits),
      callback_(callback) {}

GenerateECKeyState::GenerateECKeyState(ServiceWeakPtr weak_ptr,
                                       const std::string& named_curve,
                                       const GenerateKeyCallback& callback)
    : NSSOperationState(weak_ptr),
      named_curve_(named_curve),
      callback_(callback) {}

SignState::SignState(ServiceWeakPtr weak_ptr,
                     const std::string& data,
                     const std::string& public_key_spki_der,
                     bool raw_pkcs1,
                     HashAlgorithm hash_algorithm,
                     const KeyType key_type,
                     const SignCallback& callback)
    : NSSOperationState(weak_ptr),
      data_(data),
      public_key_spki_der_(public_key_spki_der),
      raw_pkcs1_(raw_pkcs1),
      hash_algorithm_(hash_algorithm),
      key_type_(key_type),
      callback_(callback) {}

SelectCertificatesState::SelectCertificatesState(
    ServiceWeakPtr weak_ptr,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
    const SelectCertificatesCallback& callback)
    : NSSOperationState(weak_ptr),
      cert_request_info_(cert_request_info),
      callback_(callback) {}

GetCertificatesState::GetCertificatesState(
    ServiceWeakPtr weak_ptr,
    const GetCertificatesCallback& callback)
    : NSSOperationState(weak_ptr), callback_(callback) {}

GetAllKeysState::GetAllKeysState(ServiceWeakPtr weak_ptr,
                                 GetAllKeysCallback callback)
    : NSSOperationState(weak_ptr), callback_(std::move(callback)) {}

ImportCertificateState::ImportCertificateState(
    ServiceWeakPtr weak_ptr,
    const scoped_refptr<net::X509Certificate>& certificate,
    const ImportCertificateCallback& callback)
    : NSSOperationState(weak_ptr),
      certificate_(certificate),
      callback_(callback) {}

RemoveCertificateState::RemoveCertificateState(
    ServiceWeakPtr weak_ptr,
    const scoped_refptr<net::X509Certificate>& certificate,
    const RemoveCertificateCallback& callback)
    : NSSOperationState(weak_ptr),
      certificate_(certificate),
      callback_(callback) {}

RemoveKeyState::RemoveKeyState(ServiceWeakPtr weak_ptr,
                               const std::string& public_key_spki_der,
                               RemoveKeyCallback callback)
    : NSSOperationState(weak_ptr),
      public_key_spki_der_(public_key_spki_der),
      callback_(std::move(callback)) {}

GetTokensState::GetTokensState(ServiceWeakPtr weak_ptr,
                               const GetTokensCallback& callback)
    : NSSOperationState(weak_ptr), callback_(callback) {}

GetKeyLocationsState::GetKeyLocationsState(
    ServiceWeakPtr weak_ptr,
    const std::string& public_key_spki_der,
    const GetKeyLocationsCallback& callback)
    : NSSOperationState(weak_ptr),
      public_key_spki_der_(public_key_spki_der),
      callback_(callback) {}

SetAttributeForKeyState::SetAttributeForKeyState(
    ServiceWeakPtr weak_ptr,
    const std::string& public_key_spki_der,
    CK_ATTRIBUTE_TYPE attribute_type,
    const std::string& attribute_value,
    SetAttributeForKeyCallback callback)
    : NSSOperationState(weak_ptr),
      public_key_spki_der_(public_key_spki_der),
      attribute_type_(attribute_type),
      attribute_value_(attribute_value),
      callback_(std::move(callback)) {}

GetAttributeForKeyState::GetAttributeForKeyState(
    ServiceWeakPtr weak_ptr,
    const std::string& public_key_spki_der,
    CK_ATTRIBUTE_TYPE attribute_type,
    GetAttributeForKeyCallback callback)
    : NSSOperationState(weak_ptr),
      public_key_spki_der_(public_key_spki_der),
      attribute_type_(attribute_type),
      callback_(std::move(callback)) {}

// Returns the private key corresponding to the der-encoded
// |public_key_spki_der| if found in |slot|. If |slot| is nullptr, the
// private key will be searched in all slots.
crypto::ScopedSECKEYPrivateKey GetPrivateKey(
    const std::string& public_key_spki_der,
    const crypto::ScopedPK11Slot& slot) {
  auto public_key_bytes = base::as_bytes(base::make_span(public_key_spki_der));
  if (slot) {
    return crypto::FindNSSKeyFromPublicKeyInfoInSlot(public_key_bytes,
                                                     slot.get());
  }
  return crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes);
}

// Does the actual RSA key generation on a worker thread. Used by
// GenerateRSAKeyWithDB().
void GenerateRSAKeyOnWorkerThread(std::unique_ptr<GenerateRSAKeyState> state) {
  if (!state->slot_) {
    LOG(ERROR) << "No slot.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  if (!crypto::GenerateRSAKeyPairNSS(
          state->slot_.get(), state->modulus_length_bits_, true /* permanent */,
          &public_key, &private_key)) {
    LOG(ERROR) << "Couldn't create key.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  crypto::ScopedSECItem public_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(public_key.get()));
  if (!public_key_der) {
    // TODO(https://crbug.com/1044368): Remove private_key and public_key from
    // storage.
    LOG(ERROR) << "Couldn't export public key.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }
  state->CallBack(
      FROM_HERE,
      std::string(reinterpret_cast<const char*>(public_key_der->data),
                  public_key_der->len),
      std::string() /* no error */);
}

// Does the actual EC key generation on a worker thread. Used by
// GenerateECKeyWithDB().
void GenerateECKeyOnWorkerThread(std::unique_ptr<GenerateECKeyState> state) {
  if (!state->slot_) {
    LOG(ERROR) << "No slot.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  if (state->named_curve_ != "P-256") {
    LOG(ERROR) << "Only P-256 named curve is supported.";
    state->OnError(FROM_HERE, kErrorAlgorithmNotSupported);
  }

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  if (!crypto::GenerateECKeyPairNSS(
          state->slot_.get(), SEC_OID_ANSIX962_EC_PRIME256V1,
          true /* permanent */, &public_key, &private_key)) {
    LOG(ERROR) << "Couldn't create key.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  crypto::ScopedSECItem public_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(public_key.get()));
  if (!public_key_der) {
    // TODO(https://crbug.com/1044368): Remove private_key and public_key from
    // storage.
    LOG(ERROR) << "Couldn't export public key.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }
  state->CallBack(
      FROM_HERE,
      std::string(reinterpret_cast<const char*>(public_key_der->data),
                  public_key_der->len),
      std::string() /* no error */);
}

// Continues generating a RSA key with the obtained NSSCertDatabase. Used by
// GenerateRSAKey().
void GenerateRSAKeyWithDB(std::unique_ptr<GenerateRSAKeyState> state,
                          net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task interacts with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GenerateRSAKeyOnWorkerThread, std::move(state)));
}

// Continues generating an EC key with the obtained NSSCertDatabase. Used by
// GenerateECKey().
void GenerateECKeyWithDB(std::unique_ptr<GenerateECKeyState> state,
                         net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task interacts with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GenerateECKeyOnWorkerThread, std::move(state)));
}

// Does the actual RSA signing on a worker thread.
void SignRSAOnWorkerThread(std::unique_ptr<SignState> state) {
  crypto::ScopedSECKEYPrivateKey rsa_key =
      GetPrivateKey(state->public_key_spki_der_, state->slot_);

  // Fail if the key was not found or is of the wrong type.
  if (!rsa_key || SECKEY_GetPrivateKeyType(rsa_key.get()) != rsaKey) {
    state->OnError(FROM_HERE, kErrorKeyNotFound);
    return;
  }

  std::string signature_str;
  if (state->raw_pkcs1_) {
    static_assert(
        sizeof(*state->data_.data()) == sizeof(char),
        "Can't reinterpret data if it's characters are not 8 bit large.");
    SECItem input = {siBuffer,
                     reinterpret_cast<unsigned char*>(
                         const_cast<char*>(state->data_.data())),
                     state->data_.size()};

    // Compute signature of hash.
    int signature_len = PK11_SignatureLen(rsa_key.get());
    if (signature_len <= 0) {
      state->OnError(FROM_HERE, kErrorInternal);
      return;
    }

    std::vector<unsigned char> signature(signature_len);
    SECItem signature_output = {siBuffer, signature.data(), signature.size()};
    if (PK11_Sign(rsa_key.get(), &signature_output, &input) == SECSuccess)
      signature_str.assign(signature.begin(), signature.end());
  } else {
    SECOidTag sign_alg_tag = SEC_OID_UNKNOWN;
    switch (state->hash_algorithm_) {
      case HASH_ALGORITHM_SHA1:
        sign_alg_tag = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
        break;
      case HASH_ALGORITHM_SHA256:
        sign_alg_tag = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
        break;
      case HASH_ALGORITHM_SHA384:
        sign_alg_tag = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
        break;
      case HASH_ALGORITHM_SHA512:
        sign_alg_tag = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
        break;
      case HASH_ALGORITHM_NONE:
        NOTREACHED();
        break;
    }

    crypto::ScopedSECItem sign_result(SECITEM_AllocItem(nullptr, nullptr, 0));
    if (SEC_SignData(
            sign_result.get(),
            reinterpret_cast<const unsigned char*>(state->data_.data()),
            state->data_.size(), rsa_key.get(), sign_alg_tag) == SECSuccess) {
      signature_str.assign(sign_result->data,
                           sign_result->data + sign_result->len);
    }
  }

  if (signature_str.empty()) {
    LOG(ERROR) << "Couldn't sign.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  state->CallBack(FROM_HERE, signature_str, std::string() /* no error */);
}

// Does the actual EC Signing on a worker thread.
void SignECOnWorkerThread(std::unique_ptr<SignState> state) {
  crypto::ScopedSECKEYPrivateKey ec_key =
      GetPrivateKey(state->public_key_spki_der_, state->slot_);

  // Fail if the key was not found or is of the wrong type.
  if (!ec_key || SECKEY_GetPrivateKeyType(ec_key.get()) != ecKey) {
    state->OnError(FROM_HERE, kErrorKeyNotFound);
    return;
  }

  DCHECK(!state->raw_pkcs1_);

  // Only SHA-256 algorithm is supported for ECDSA.
  if (state->hash_algorithm_ != HASH_ALGORITHM_SHA256) {
    state->OnError(FROM_HERE, kErrorAlgorithmNotSupported);
    return;
  }

  std::string signature_str;
  SECOidTag sign_alg_tag = SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE;
  crypto::ScopedSECItem sign_result(SECITEM_AllocItem(nullptr, nullptr, 0));
  if (SEC_SignData(sign_result.get(),
                   reinterpret_cast<const unsigned char*>(state->data_.data()),
                   state->data_.size(), ec_key.get(),
                   sign_alg_tag) != SECSuccess) {
    LOG(ERROR) << "Couldn't sign using elliptic curve key.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  int signature_len = PK11_SignatureLen(ec_key.get());
  crypto::ScopedSECItem web_crypto_signature(
      DSAU_DecodeDerSigToLen(sign_result.get(), signature_len));

  if (!web_crypto_signature || web_crypto_signature->len == 0) {
    LOG(ERROR) << "Couldn't convert signature to WebCrypto format.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  signature_str.assign(web_crypto_signature->data,
                       web_crypto_signature->data + web_crypto_signature->len);

  state->CallBack(FROM_HERE, signature_str, std::string() /* no error */);
}

// Decides which signing algorithm will be used. Used by SignWithDB().
void SignOnWorkerThread(std::unique_ptr<SignState> state) {
  crypto::ScopedSECKEYPrivateKey key =
      GetPrivateKey(state->public_key_spki_der_, state->slot_);

  if (!key) {
    state->OnError(FROM_HERE, kErrorKeyNotFound);
    return;
  }

  switch (state->key_type_) {
    case KeyType::kRsassaPkcs1V15:
      SignRSAOnWorkerThread(std::move(state));
      break;
    case KeyType::kEcdsa:
      SignECOnWorkerThread(std::move(state));
      break;
  }
}

// Continues signing with the obtained NSSCertDatabase. Used by Sign().
void SignWithDB(std::unique_ptr<SignState> state,
                net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task interacts with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SignOnWorkerThread, std::move(state)));
}

// Called when ClientCertStoreChromeOS::GetClientCerts is done. Builds the list
// of net::CertificateList and calls back. Used by SelectCertificates().
void DidSelectCertificates(std::unique_ptr<SelectCertificatesState> state,
                           net::ClientCertIdentityList identities) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Convert the ClientCertIdentityList to a CertificateList since returning
  // ClientCertIdentities would require changing the platformKeys extension
  // api. This assumes that the necessary keys can be found later with
  // crypto::FindNSSKeyFromPublicKeyInfo.
  auto certs = std::make_unique<net::CertificateList>();
  for (const std::unique_ptr<net::ClientCertIdentity>& identity : identities)
    certs->push_back(identity->certificate());
  // DidSelectCertificates() may be called synchronously, so run the callback on
  // a separate event loop iteration to avoid potential reentrancy bugs.
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&SelectCertificatesState::CallBack,
                                std::move(state), FROM_HERE, std::move(certs),
                                std::string() /* no error */));
}

// Filters the obtained certificates on a worker thread. Used by
// DidGetCertificates().
void FilterCertificatesOnWorkerThread(
    std::unique_ptr<GetCertificatesState> state) {
  std::unique_ptr<net::CertificateList> client_certs(new net::CertificateList);
  for (net::ScopedCERTCertificateList::const_iterator it =
           state->certs_.begin();
       it != state->certs_.end(); ++it) {
    CERTCertificate* cert_handle = it->get();
    crypto::ScopedPK11Slot cert_slot(PK11_KeyForCertExists(cert_handle,
                                                           nullptr,    // keyPtr
                                                           nullptr));  // wincx

    // Keep only user certificates, i.e. certs for which the private key is
    // present and stored in the queried slot.
    if (cert_slot != state->slot_)
      continue;

    // Allow UTF-8 inside PrintableStrings in client certificates. See
    // crbug.com/770323 and crbug.com/788655.
    net::X509Certificate::UnsafeCreateOptions options;
    options.printable_string_is_utf8 = true;
    scoped_refptr<net::X509Certificate> cert =
        net::x509_util::CreateX509CertificateFromCERTCertificate(cert_handle,
                                                                 {}, options);
    if (!cert)
      continue;

    client_certs->push_back(std::move(cert));
  }

  state->CallBack(FROM_HERE, std::move(client_certs),
                  std::string() /* no error */);
}

// Passes the obtained certificates to the worker thread for filtering. Used by
// GetCertificatesWithDB().
void DidGetCertificates(std::unique_ptr<GetCertificatesState> state,
                        net::ScopedCERTCertificateList all_certs) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  state->certs_ = std::move(all_certs);
  // This task interacts with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FilterCertificatesOnWorkerThread, std::move(state)));
}

// Continues getting certificates with the obtained NSSCertDatabase. Used by
// GetCertificates().
void GetCertificatesWithDB(std::unique_ptr<GetCertificatesState> state,
                           net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Get the pointer to slot before base::Passed releases |state|.
  PK11SlotInfo* slot = state->slot_.get();
  cert_db->ListCertsInSlot(
      base::BindOnce(&DidGetCertificates, std::move(state)), slot);
}

// Does the actual retrieval of the SubjectPublicKeyInfo string on a worker
// thread. Used by GetAllKeysWithDb().
void GetAllKeysOnWorkerThread(std::unique_ptr<GetAllKeysState> state) {
  DCHECK(state->slot_.get());

  std::vector<std::string> public_key_spki_der_list;

  // This assumes that all public keys on the slots are actually key pairs with
  // private + public keys, so it's sufficient to get the public keys (and also
  // not necessary to check that a private key for that public key really
  // exists).
  SECKEYPublicKeyList* public_keys =
      PK11_ListPublicKeysInSlot(state->slot_.get(), /*nickname=*/nullptr);

  if (!public_keys) {
    state->CallBack(FROM_HERE, std::move(public_key_spki_der_list),
                    std::string() /* no error */);
    return;
  }

  for (SECKEYPublicKeyListNode* node = PUBKEY_LIST_HEAD(public_keys);
       !PUBKEY_LIST_END(node, public_keys); node = PUBKEY_LIST_NEXT(node)) {
    crypto::ScopedSECItem subject_public_key_info(
        SECKEY_EncodeDERSubjectPublicKeyInfo(node->key));
    if (!subject_public_key_info) {
      LOG(WARNING) << "Could not encode subject public key info.";
      continue;
    }

    if (subject_public_key_info->len > 0) {
      public_key_spki_der_list.push_back(std::string(
          subject_public_key_info->data,
          subject_public_key_info->data + subject_public_key_info->len));
    }
  }

  SECKEY_DestroyPublicKeyList(public_keys);
  state->CallBack(FROM_HERE, std::move(public_key_spki_der_list),
                  std::string() /* no error */);
}

// Continues the retrieval of the SubjectPublicKeyInfo list with |cert_db|.
// Used by GetAllKeys().
void GetAllKeysWithDb(std::unique_ptr<GetAllKeysState> state,
                      net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GetAllKeysOnWorkerThread, std::move(state)));
}

// Does the actual certificate importing on the IO thread. Used by
// ImportCertificate().
void ImportCertificateWithDB(std::unique_ptr<ImportCertificateState> state,
                             net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!state->certificate_) {
    state->OnError(FROM_HERE, net::ErrorToString(net::ERR_CERT_INVALID));
    return;
  }
  if (state->certificate_->HasExpired()) {
    state->OnError(FROM_HERE, net::ErrorToString(net::ERR_CERT_DATE_INVALID));
    return;
  }

  net::ScopedCERTCertificate nss_cert =
      net::x509_util::CreateCERTCertificateFromX509Certificate(
          state->certificate_.get());
  if (!nss_cert) {
    state->OnError(FROM_HERE, net::ErrorToString(net::ERR_CERT_INVALID));
    return;
  }

  // Check that the private key is in the correct slot.
  crypto::ScopedPK11Slot slot(
      PK11_KeyForCertExists(nss_cert.get(), nullptr, nullptr));
  if (slot.get() != state->slot_.get()) {
    state->OnError(FROM_HERE, kErrorKeyNotFound);
    return;
  }

  const net::Error import_status =
      static_cast<net::Error>(cert_db->ImportUserCert(nss_cert.get()));
  if (import_status != net::OK) {
    LOG(ERROR) << "Could not import certificate.";
    state->OnError(FROM_HERE, net::ErrorToString(import_status));
    return;
  }

  state->CallBack(FROM_HERE, std::string() /* no error */);
}

// Called on IO thread after the certificate removal is finished.
void DidRemoveCertificate(std::unique_ptr<RemoveCertificateState> state,
                          bool certificate_found,
                          bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // CertificateNotFound error has precedence over an internal error.
  if (!certificate_found) {
    state->OnError(FROM_HERE, kErrorCertificateNotFound);
    return;
  }
  if (!success) {
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  state->CallBack(FROM_HERE, std::string() /* no error */);
}

// Does the actual certificate removal on the IO thread. Used by
// RemoveCertificate().
void RemoveCertificateWithDB(std::unique_ptr<RemoveCertificateState> state,
                             net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  net::ScopedCERTCertificate nss_cert =
      net::x509_util::CreateCERTCertificateFromX509Certificate(
          state->certificate_.get());
  if (!nss_cert) {
    state->OnError(FROM_HERE, net::ErrorToString(net::ERR_CERT_INVALID));
    return;
  }

  bool certificate_found = nss_cert->isperm;
  cert_db->DeleteCertAndKeyAsync(
      std::move(nss_cert),
      base::BindOnce(&DidRemoveCertificate, base::Passed(&state),
                     certificate_found));
}

// Does the actual key pair removal on a worker thread. Used by
// RemoveKeyWithDb().
void RemoveKeyOnWorkerThread(std::unique_ptr<RemoveKeyState> state) {
  DCHECK(state->slot_.get());

  crypto::ScopedSECKEYPrivateKey private_key =
      GetPrivateKey(state->public_key_spki_der_, state->slot_);

  if (!private_key) {
    state->OnError(FROM_HERE, kErrorKeyNotFound);
    return;
  }

  crypto::ScopedSECKEYPublicKey public_key(
      SECKEY_ConvertToPublicKey(private_key.get()));

  // PK11_DeleteTokenPrivateKey function frees the privKey structure
  // unconditionally, and thus releasing the ownership of the passed private
  // key.
  // |force| is set to false so as not to delete the key if there are any
  // matching certificates.
  if (PK11_DeleteTokenPrivateKey(/*privKey=*/private_key.release(),
                                 /*force=*/false) != SECSuccess) {
    LOG(ERROR) << "Cannot delete private key";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  // PK11_DeleteTokenPublicKey function frees the pubKey structure
  // unconditionally, and thus releasing the ownership of the passed private
  // key.
  if (PK11_DeleteTokenPublicKey(/*pubKey=*/public_key.release()) !=
      SECSuccess) {
    LOG(WARNING) << "Cannot delete public key";
  }

  state->CallBack(FROM_HERE, std::string() /* no error */);
}

// Continues removing the key pair with the obtained |cert_db|. Called by
// RemoveKey().
void RemoveKeyWithDb(std::unique_ptr<RemoveKeyState> state,
                     net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&RemoveKeyOnWorkerThread, std::move(state)));
}

// Does the actual work to determine which tokens are available.
void GetTokensWithDB(std::unique_ptr<GetTokensState> state,
                     net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::unique_ptr<std::vector<std::string>> token_ids(
      new std::vector<std::string>);

  // The user token will be unavailable in case of no logged in user in this
  // profile.
  if (cert_db->GetPrivateSlot())
    token_ids->push_back(kTokenIdUser);

  if (cert_db->GetSystemSlot())
    token_ids->push_back(kTokenIdSystem);

  DCHECK(!token_ids->empty());

  state->CallBack(FROM_HERE, std::move(token_ids),
                  std::string() /* no error */);
}

// Does the actual work to determine which key is on which token.
void GetKeyLocationsWithDB(std::unique_ptr<GetKeyLocationsState> state,
                           net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<std::string> token_ids;

  const uint8_t* public_key_uint8 =
      reinterpret_cast<const uint8_t*>(state->public_key_spki_der_.data());
  std::vector<uint8_t> public_key_vector(
      public_key_uint8, public_key_uint8 + state->public_key_spki_der_.size());

  if (cert_db->GetPrivateSlot().get()) {
    crypto::ScopedSECKEYPrivateKey rsa_key =
        crypto::FindNSSKeyFromPublicKeyInfoInSlot(
            public_key_vector, cert_db->GetPrivateSlot().get());
    if (rsa_key)
      token_ids.push_back(kTokenIdUser);
  }
  if (token_ids.empty() && cert_db->GetPublicSlot().get()) {
    crypto::ScopedSECKEYPrivateKey rsa_key =
        crypto::FindNSSKeyFromPublicKeyInfoInSlot(
            public_key_vector, cert_db->GetPublicSlot().get());
    if (rsa_key)
      token_ids.push_back(kTokenIdUser);
  }

  if (cert_db->GetSystemSlot().get()) {
    crypto::ScopedSECKEYPrivateKey rsa_key =
        crypto::FindNSSKeyFromPublicKeyInfoInSlot(
            public_key_vector, cert_db->GetSystemSlot().get());
    if (rsa_key)
      token_ids.push_back(kTokenIdSystem);
  }

  state->CallBack(FROM_HERE, std::move(token_ids),
                  std::string() /* no error */);
}

// Translates |type| to one of the NSS softoken module's predefined key
// attributes which are used in tests.
CK_ATTRIBUTE_TYPE TranslateKeyAttributeTypeForSoftoken(KeyAttributeType type) {
  switch (type) {
    case KeyAttributeType::CertificateProvisioningId:
      return CKA_START_DATE;
  }
}

// If |map_to_softoken_attrs| is true, translates |type| to one of the softoken
// module predefined key attributes. Otherwise, applies normal translation.
CK_ATTRIBUTE_TYPE TranslateKeyAttributeType(KeyAttributeType type,
                                            bool map_to_softoken_attrs) {
  if (map_to_softoken_attrs) {
    return TranslateKeyAttributeTypeForSoftoken(type);
  }

  switch (type) {
    case KeyAttributeType::CertificateProvisioningId:
      return pkcs11_custom_attributes::kCkaChromeOsBuiltinProvisioningProfileId;
  }
}

// Does the actual attribute value setting with the obtained |cert_db|.
// Called by SetAttributeForKey().
void SetAttributeForKeyWithDb(std::unique_ptr<SetAttributeForKeyState> state,
                              net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(state->slot_.get());

  crypto::ScopedSECKEYPrivateKey private_key =
      GetPrivateKey(state->public_key_spki_der_, state->slot_);

  if (!private_key) {
    state->OnError(FROM_HERE, kErrorKeyNotFound);
    return;
  }

  // This SECItem will point to data owned by |state| so it is not necessary to
  // use ScopedSECItem.
  SECItem attribute_value;

  attribute_value.data = reinterpret_cast<unsigned char*>(
      const_cast<char*>(state->attribute_value_.data()));
  attribute_value.len = state->attribute_value_.size();

  if (PK11_WriteRawAttribute(
          /*objType=*/PK11_TypePrivKey, private_key.get(),
          state->attribute_type_, &attribute_value) != SECSuccess) {
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  state->CallBack(FROM_HERE, std::string() /* no error */);
}

// Does the actual attribute value retrieval with the obtained |cert_db|.
// Called by GetAttributeForKey().
void GetAttributeForKeyWithDb(std::unique_ptr<GetAttributeForKeyState> state,
                              net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(state->slot_.get());

  crypto::ScopedSECKEYPrivateKey private_key =
      GetPrivateKey(state->public_key_spki_der_, state->slot_);

  if (!private_key) {
    state->OnError(FROM_HERE, kErrorKeyNotFound);
    return;
  }

  crypto::ScopedSECItem attribute_value(SECITEM_AllocItem(/*arena=*/nullptr,
                                                          /*item=*/nullptr,
                                                          /*len=*/0));
  DCHECK(attribute_value.get());

  if (PK11_ReadRawAttribute(
          /*objType=*/PK11_TypePrivKey, private_key.get(),
          state->attribute_type_, attribute_value.get()) != SECSuccess) {
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  std::string attribute_value_str;
  if (attribute_value->len > 0) {
    attribute_value_str.assign(attribute_value->data,
                               attribute_value->data + attribute_value->len);
  }

  state->CallBack(FROM_HERE, attribute_value_str, std::string() /* no error */);
}

}  // namespace

void PlatformKeysServiceImpl::GenerateRSAKey(
    const std::string& token_id,
    unsigned int modulus_length_bits,
    const GenerateKeyCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<GenerateRSAKeyState>(
      weak_factory_.GetWeakPtr(), modulus_length_bits, callback);

  if (modulus_length_bits > kMaxRSAModulusLengthBits) {
    state->OnError(FROM_HERE, kErrorAlgorithmNotSupported);
    return;
  }

  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id,
                  base::Bind(&GenerateRSAKeyWithDB, base::Passed(&state)),
                  browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::GenerateECKey(
    const std::string& token_id,
    const std::string& named_curve,
    const GenerateKeyCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<GenerateECKeyState>(weak_factory_.GetWeakPtr(),
                                                    named_curve, callback);

  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id,
                  base::Bind(&GenerateECKeyWithDB, base::Passed(&state)),
                  browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::SignRSAPKCS1Digest(
    const std::string& token_id,
    const std::string& data,
    const std::string& public_key_spki_der,
    HashAlgorithm hash_algorithm,
    const SignCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<SignState>(
      weak_factory_.GetWeakPtr(), data, public_key_spki_der,
      /*raw_pkcs1=*/false, hash_algorithm,
      /*key_type=*/KeyType::kRsassaPkcs1V15, callback);

  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages and we can double check that
  // we use a key of the correct token.
  GetCertDatabase(token_id, base::Bind(&SignWithDB, base::Passed(&state)),
                  browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::SignRSAPKCS1Raw(
    const std::string& token_id,
    const std::string& data,
    const std::string& public_key_spki_der,
    const SignCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<SignState>(
      weak_factory_.GetWeakPtr(), data, public_key_spki_der,
      /*raw_pkcs1=*/true, HASH_ALGORITHM_NONE,
      /*key_type=*/KeyType::kRsassaPkcs1V15, callback);

  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages and we can double check that
  // we use a key of the correct token.
  GetCertDatabase(token_id, base::Bind(&SignWithDB, base::Passed(&state)),
                  browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::SignECDSADigest(
    const std::string& token_id,
    const std::string& data,
    const std::string& public_key_spki_der,
    HashAlgorithm hash_algorithm,
    const SignCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<SignState>(
      weak_factory_.GetWeakPtr(), data, public_key_spki_der,
      /*raw_pkcs1=*/false, hash_algorithm,
      /*key_type=*/KeyType::kEcdsa, callback);

  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages and we can double check that
  // we use a key of the correct token.
  GetCertDatabase(token_id, base::Bind(&SignWithDB, base::Passed(&state)),
                  browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::SelectClientCertificates(
    const std::vector<std::string>& certificate_authorities,
    const SelectCertificatesCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto cert_request_info = base::MakeRefCounted<net::SSLCertRequestInfo>();

  // Currently we do not pass down the requested certificate type to the net
  // layer, as it does not support filtering certificates by type. Rather, we
  // do not constrain the certificate type here, instead the caller has to apply
  // filtering afterwards.
  cert_request_info->cert_authorities = certificate_authorities;

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          Profile::FromBrowserContext(browser_context_));

  // Use the device-wide system key slot only if the user is affiliated on the
  // device.
  const bool use_system_key_slot = user->IsAffiliated();

  auto state = std::make_unique<SelectCertificatesState>(
      weak_factory_.GetWeakPtr(), cert_request_info, callback);

  state->cert_store_ = std::make_unique<ClientCertStoreChromeOS>(
      nullptr,  // no additional provider
      use_system_key_slot, user->username_hash(),
      ClientCertStoreChromeOS::PasswordDelegateFactory());

  // Note DidSelectCertificates() may be called synchronously.
  SelectCertificatesState* state_ptr = state.get();
  state_ptr->cert_store_->GetClientCerts(
      *state_ptr->cert_request_info_,
      base::BindOnce(&DidSelectCertificates, std::move(state)));
}

std::string GetSubjectPublicKeyInfo(
    const scoped_refptr<net::X509Certificate>& certificate) {
  base::StringPiece spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
          &spki_bytes))
    return {};
  return spki_bytes.as_string();
}

bool GetPublicKey(const scoped_refptr<net::X509Certificate>& certificate,
                  net::X509Certificate::PublicKeyType* key_type,
                  size_t* key_size_bits) {
  net::X509Certificate::PublicKeyType key_type_tmp =
      net::X509Certificate::kPublicKeyTypeUnknown;
  size_t key_size_bits_tmp = 0;
  net::X509Certificate::GetPublicKeyInfo(certificate->cert_buffer(),
                                         &key_size_bits_tmp, &key_type_tmp);

  if (key_type_tmp == net::X509Certificate::kPublicKeyTypeUnknown) {
    LOG(WARNING) << "Could not extract public key of certificate.";
    return false;
  }
  if (key_type_tmp != net::X509Certificate::kPublicKeyTypeRSA) {
    LOG(WARNING) << "Keys of other type than RSA are not supported.";
    return false;
  }

  std::string spki = GetSubjectPublicKeyInfo(certificate);
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(spki.data()), spki.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
  if (!pkey) {
    LOG(WARNING) << "Could not extract public key of certificate.";
    return false;
  }
  RSA* rsa = EVP_PKEY_get0_RSA(pkey.get());
  if (!rsa) {
    LOG(WARNING) << "Could not get RSA from PKEY.";
    return false;
  }

  const BIGNUM* public_exponent;
  RSA_get0_key(rsa, nullptr /* out_n */, &public_exponent, nullptr /* out_d */);
  if (BN_get_word(public_exponent) != 65537L) {
    LOG(ERROR) << "Rejecting RSA public exponent that is unequal 65537.";
    return false;
  }

  *key_type = key_type_tmp;
  *key_size_bits = key_size_bits_tmp;
  return true;
}

void PlatformKeysServiceImpl::GetCertificates(
    const std::string& token_id,
    const GetCertificatesCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<GetCertificatesState>(
      weak_factory_.GetWeakPtr(), callback);
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id,
                  base::Bind(&GetCertificatesWithDB, base::Passed(&state)),
                  browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::GetAllKeys(const std::string& token_id,
                                         GetAllKeysCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto state = std::make_unique<GetAllKeysState>(weak_factory_.GetWeakPtr(),
                                                 std::move(callback));

  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id,
                  base::BindRepeating(&GetAllKeysWithDb, base::Passed(&state)),
                  browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::ImportCertificate(
    const std::string& token_id,
    const scoped_refptr<net::X509Certificate>& certificate,
    const ImportCertificateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<ImportCertificateState>(
      weak_factory_.GetWeakPtr(), certificate, callback);
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages and we can double check that
  // we use a key of the correct token.
  GetCertDatabase(token_id,
                  base::Bind(&ImportCertificateWithDB, base::Passed(&state)),
                  browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::RemoveCertificate(
    const std::string& token_id,
    const scoped_refptr<net::X509Certificate>& certificate,
    const RemoveCertificateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<RemoveCertificateState>(
      weak_factory_.GetWeakPtr(), certificate, callback);
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages.
  GetCertDatabase(token_id,
                  base::Bind(&RemoveCertificateWithDB, base::Passed(&state)),
                  browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::RemoveKey(const std::string& token_id,
                                        const std::string& public_key_spki_der,
                                        RemoveKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto state = std::make_unique<RemoveKeyState>(
      weak_factory_.GetWeakPtr(), public_key_spki_der, std::move(callback));

  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages.
  GetCertDatabase(token_id, base::Bind(&RemoveKeyWithDb, base::Passed(&state)),
                  browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::GetTokens(const GetTokensCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state =
      std::make_unique<GetTokensState>(weak_factory_.GetWeakPtr(), callback);
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(std::string() /* don't get any specific slot */,
                  base::Bind(&GetTokensWithDB, base::Passed(&state)),
                  browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::GetKeyLocations(
    const std::string& public_key_spki_der,
    const GetKeyLocationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<GetKeyLocationsState>(
      weak_factory_.GetWeakPtr(), public_key_spki_der, callback);
  NSSOperationState* state_ptr = state.get();

  GetCertDatabase(
      std::string() /* don't get any specific slot - we need all slots */,
      base::BindRepeating(&GetKeyLocationsWithDB, base::Passed(&state)),
      browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::SetAttributeForKey(
    const std::string& token_id,
    const std::string& public_key_spki_der,
    KeyAttributeType attribute_type,
    const std::string& attribute_value,
    SetAttributeForKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CK_ATTRIBUTE_TYPE ck_attribute_type = TranslateKeyAttributeType(
      attribute_type,
      /*map_to_softoken_attrs=*/IsSetMapToSoftokenAttrsForTesting());

  auto state = std::make_unique<SetAttributeForKeyState>(
      weak_factory_.GetWeakPtr(), public_key_spki_der, ck_attribute_type,
      attribute_value, std::move(callback));

  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. Only setting the state slot is
  // required.
  GetCertDatabase(
      token_id,
      base::BindRepeating(&SetAttributeForKeyWithDb, base::Passed(&state)),
      browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::GetAttributeForKey(
    const std::string& token_id,
    const std::string& public_key_spki_der,
    KeyAttributeType attribute_type,
    GetAttributeForKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CK_ATTRIBUTE_TYPE ck_attribute_type = TranslateKeyAttributeType(
      attribute_type,
      /*map_to_softoken_attrs=*/IsSetMapToSoftokenAttrsForTesting());

  auto state = std::make_unique<GetAttributeForKeyState>(
      weak_factory_.GetWeakPtr(), public_key_spki_der, ck_attribute_type,
      std::move(callback));

  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. Only setting the state slot is
  // required.
  GetCertDatabase(
      token_id,
      base::BindRepeating(&GetAttributeForKeyWithDb, base::Passed(&state)),
      browser_context_, state_ptr);
}

void PlatformKeysServiceImpl::SetMapToSoftokenAttrsForTesting(
    bool map_to_softoken_attrs_for_testing) {
  map_to_softoken_attrs_for_testing_ = map_to_softoken_attrs_for_testing;
}

bool PlatformKeysServiceImpl::IsSetMapToSoftokenAttrsForTesting() {
  return map_to_softoken_attrs_for_testing_;
}

}  // namespace platform_keys
}  // namespace chromeos
