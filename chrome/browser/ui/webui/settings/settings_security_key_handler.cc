// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_security_key_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/service_manager_connection.h"
#include "device/fido/credential_management.h"
#include "device/fido/credential_management_handler.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/pin.h"
#include "device/fido/reset_request_handler.h"
#include "device/fido/set_pin_request_handler.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

base::flat_set<device::FidoTransportProtocol> supported_transports() {
  // If we ever support BLE devices then additional thought will be required
  // in the UI; therefore don't enable them here. NFC is not supported on
  // desktop thus only USB devices remain to be enabled.
  return {device::FidoTransportProtocol::kUsbHumanInterfaceDevice};
}

void HandleClose(base::Closure close_callback, const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());
  close_callback.Run();
}

}  // namespace

namespace settings {

void SecurityKeysHandlerBase::OnJavascriptAllowed() {}

void SecurityKeysHandlerBase::OnJavascriptDisallowed() {
  // If Javascript is disallowed, |Close| will invalidate all current WeakPtrs
  // and thus drop all pending callbacks. This means that
  // |IsJavascriptAllowed| doesn't need to be tested before each callback
  // because, if the callback into this object happened, then Javascript is
  // allowed.
  Close();
}

SecurityKeysPINHandler::SecurityKeysPINHandler() = default;
SecurityKeysPINHandler::~SecurityKeysPINHandler() = default;

void SecurityKeysPINHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "securityKeyStartSetPIN",
      base::BindRepeating(&SecurityKeysPINHandler::HandleStartSetPIN,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeySetPIN",
      base::BindRepeating(&SecurityKeysPINHandler::HandleSetPIN,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyPINClose",
      base::BindRepeating(&HandleClose,
                          base::BindRepeating(&SecurityKeysPINHandler::Close,
                                              base::Unretained(this))));
}

void SecurityKeysPINHandler::Close() {
  // Invalidate all existing WeakPtrs so that no stale callbacks occur.
  weak_factory_.InvalidateWeakPtrs();
  state_ = State::kNone;
  set_pin_.reset();
  callback_id_.clear();
}

void SecurityKeysPINHandler::HandleStartSetPIN(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(State::kNone, state_);
  DCHECK_EQ(1u, args->GetSize());

  AllowJavascript();
  DCHECK(callback_id_.empty());
  callback_id_ = args->GetList()[0].GetString();
  state_ = State::kStartSetPIN;
  set_pin_ = std::make_unique<device::SetPINRequestHandler>(
      content::GetSystemConnector(), supported_transports(),
      base::BindOnce(&SecurityKeysPINHandler::OnGatherPIN,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SecurityKeysPINHandler::OnSetPINComplete,
                          weak_factory_.GetWeakPtr()));
}

void SecurityKeysPINHandler::OnGatherPIN(base::Optional<int64_t> num_retries) {
  DCHECK_EQ(State::kStartSetPIN, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::Value::ListStorage list;
  list.emplace_back(0 /* process not complete */);
  if (num_retries) {
    state_ = State::kGatherChangePIN;
    list.emplace_back(static_cast<int>(*num_retries));
  } else {
    state_ = State::kGatherNewPIN;
    list.emplace_back(base::Value::Type::NONE);
  }

  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::Value(std::move(list)));
}

void SecurityKeysPINHandler::OnSetPINComplete(
    device::CtapDeviceResponseCode code) {
  DCHECK(state_ == State::kStartSetPIN || state_ == State::kSettingPIN);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (code == device::CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    // In the event that the old PIN was incorrect, the UI may prompt again.
    state_ = State::kGatherChangePIN;
  } else {
    state_ = State::kNone;
    set_pin_.reset();
  }

  base::Value::ListStorage list;
  list.emplace_back(1 /* process complete */);
  list.emplace_back(static_cast<int>(code));
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::Value(std::move(list)));
}

void SecurityKeysPINHandler::HandleSetPIN(const base::ListValue* args) {
  DCHECK(state_ == State::kGatherNewPIN || state_ == State::kGatherChangePIN);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(3u, args->GetSize());

  DCHECK(callback_id_.empty());
  callback_id_ = args->GetList()[0].GetString();
  const std::string old_pin = args->GetList()[1].GetString();
  const std::string new_pin = args->GetList()[2].GetString();

  DCHECK((state_ == State::kGatherNewPIN) == old_pin.empty());

  CHECK(device::pin::IsValid(new_pin));
  state_ = State::kSettingPIN;
  set_pin_->ProvidePIN(old_pin, new_pin);
}

SecurityKeysResetHandler::SecurityKeysResetHandler() = default;
SecurityKeysResetHandler::~SecurityKeysResetHandler() = default;

void SecurityKeysResetHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "securityKeyReset",
      base::BindRepeating(&SecurityKeysResetHandler::HandleReset,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCompleteReset",
      base::BindRepeating(&SecurityKeysResetHandler::HandleCompleteReset,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyResetClose",
      base::BindRepeating(&HandleClose,
                          base::BindRepeating(&SecurityKeysResetHandler::Close,
                                              base::Unretained(this))));
}

void SecurityKeysResetHandler::Close() {
  // Invalidate all existing WeakPtrs so that no stale callbacks occur.
  weak_factory_.InvalidateWeakPtrs();
  state_ = State::kNone;
  reset_.reset();
  callback_id_.clear();
}

void SecurityKeysResetHandler::HandleReset(const base::ListValue* args) {
  DCHECK_EQ(State::kNone, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args->GetSize());

  AllowJavascript();
  DCHECK(callback_id_.empty());
  callback_id_ = args->GetList()[0].GetString();

  state_ = State::kStartReset;
  reset_ = std::make_unique<device::ResetRequestHandler>(
      content::GetSystemConnector(), supported_transports(),
      base::BindOnce(&SecurityKeysResetHandler::OnResetSent,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&SecurityKeysResetHandler::OnResetFinished,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeysResetHandler::OnResetSent() {
  DCHECK_EQ(State::kStartReset, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // A reset message has been sent to a security key and it may complete
  // before Javascript asks for the result. Therefore |HandleCompleteReset|
  // and |OnResetFinished| may be called in either order.
  state_ = State::kWaitingForResetNoCallbackYet;
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::Value(0 /* success */));
}

void SecurityKeysResetHandler::HandleCompleteReset(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args->GetSize());

  DCHECK(callback_id_.empty());
  callback_id_ = args->GetList()[0].GetString();

  switch (state_) {
    case State::kWaitingForResetNoCallbackYet:
      // The reset operation hasn't completed. |callback_id_| will be used in
      // |OnResetFinished| when it does.
      state_ = State::kWaitingForResetHaveCallback;
      break;

    case State::kWaitingForCompleteReset:
      // The reset operation has completed and we were waiting for this
      // call from Javascript in order to provide the result.
      state_ = State::kNone;
      ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                                base::Value(static_cast<int>(*reset_result_)));
      reset_.reset();
      break;

    default:
      NOTREACHED();
  }
}

void SecurityKeysResetHandler::OnResetFinished(
    device::CtapDeviceResponseCode result) {
  switch (state_) {
    case State::kWaitingForResetNoCallbackYet:
      // The reset operation has completed, but Javascript hasn't called
      // |CompleteReset| so we cannot make the callback yet.
      state_ = State::kWaitingForCompleteReset;
      reset_result_ = result;
      break;

    case State::kStartReset:
      // The reset operation failed immediately, probably because the user
      // selected a U2F device. |callback_id_| has been set by |Reset|.
      [[fallthrough]];

    case State::kWaitingForResetHaveCallback:
      // The |CompleteReset| call has already provided |callback_id_| so the
      // reset can be completed immediately.
      state_ = State::kNone;
      ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                                base::Value(static_cast<int>(result)));
      reset_.reset();
      break;

    default:
      NOTREACHED();
  }
}

SecurityKeysCredentialHandler::SecurityKeysCredentialHandler() = default;
SecurityKeysCredentialHandler::~SecurityKeysCredentialHandler() = default;

void SecurityKeysCredentialHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementStart",
      base::BindRepeating(&SecurityKeysCredentialHandler::HandleStart,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementPIN",
      base::BindRepeating(&SecurityKeysCredentialHandler::HandlePIN,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementEnumerate",
      base::BindRepeating(&SecurityKeysCredentialHandler::HandleEnumerate,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementDelete",
      base::BindRepeating(&SecurityKeysCredentialHandler::HandleDelete,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementClose",
      base::BindRepeating(
          &HandleClose,
          base::BindRepeating(&SecurityKeysCredentialHandler::Close,
                              base::Unretained(this))));
}

void SecurityKeysCredentialHandler::Close() {
  // Invalidate all existing WeakPtrs so that no stale callbacks occur.
  weak_factory_.InvalidateWeakPtrs();
  state_ = State::kNone;
  discovery_factory_.reset();
  credential_management_.reset();
  callback_id_.clear();
  credential_management_provide_pin_cb_.Reset();
  DCHECK(!credential_management_provide_pin_cb_);
}

void SecurityKeysCredentialHandler::HandleStart(const base::ListValue* args) {
  DCHECK_EQ(State::kNone, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args->GetSize());
  DCHECK(!credential_management_);
  DCHECK(!discovery_factory_);

  AllowJavascript();
  DCHECK(callback_id_.empty());
  callback_id_ = args->GetList()[0].GetString();

  state_ = State::kStart;
  discovery_factory_ = std::make_unique<device::FidoDiscoveryFactory>();
  credential_management_ =
      std::make_unique<device::CredentialManagementHandler>(
          content::ServiceManagerConnection::GetForProcess()->GetConnector(),
          discovery_factory_.get(), supported_transports(),
          base::BindOnce(
              &SecurityKeysCredentialHandler::OnCredentialManagementReady,
              weak_factory_.GetWeakPtr()),
          base::BindRepeating(&SecurityKeysCredentialHandler::OnGatherPIN,
                              weak_factory_.GetWeakPtr()),
          base::BindOnce(&SecurityKeysCredentialHandler::OnFinished,
                         weak_factory_.GetWeakPtr()));
}

void SecurityKeysCredentialHandler::HandlePIN(const base::ListValue* args) {
  DCHECK_EQ(State::kPIN, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(2u, args->GetSize());
  DCHECK(credential_management_);
  DCHECK(credential_management_provide_pin_cb_);
  DCHECK(callback_id_.empty());

  callback_id_ = args->GetList()[0].GetString();
  std::string pin = args->GetList()[1].GetString();

  std::move(credential_management_provide_pin_cb_).Run(pin);
}

void SecurityKeysCredentialHandler::HandleEnumerate(
    const base::ListValue* args) {
  DCHECK_EQ(state_, State::kReady);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args->GetSize());
  DCHECK(credential_management_);
  DCHECK(callback_id_.empty());

  state_ = State::kGettingCredentials;
  callback_id_ = args->GetList()[0].GetString();
  credential_management_->GetCredentials(
      base::BindOnce(&SecurityKeysCredentialHandler::OnHaveCredentials,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeysCredentialHandler::HandleDelete(const base::ListValue* args) {
  DCHECK_EQ(State::kReady, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(2u, args->GetSize());
  DCHECK(credential_management_);
  DCHECK(callback_id_.empty());

  state_ = State::kDeletingCredentials;
  callback_id_ = args->GetList()[0].GetString();
  std::vector<std::vector<uint8_t>> credential_ids;
  for (const base::Value& el : args->GetList()[1].GetList()) {
    std::vector<uint8_t> credential_id;
    if (!base::HexStringToBytes(el.GetString(), &credential_id)) {
      NOTREACHED();
      continue;
    }
    credential_ids.emplace_back(std::move(credential_id));
  }
  credential_management_->DeleteCredentials(
      std::move(credential_ids),
      base::BindOnce(&SecurityKeysCredentialHandler::OnCredentialsDeleted,
                     weak_factory_.GetWeakPtr()));
}
void SecurityKeysCredentialHandler::OnCredentialManagementReady() {
  DCHECK(state_ == State::kStart || state_ == State::kPIN);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(credential_management_);
  DCHECK(!callback_id_.empty());

  state_ = State::kReady;
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::Value());
}

void SecurityKeysCredentialHandler::OnHaveCredentials(
    device::CtapDeviceResponseCode status,
    base::Optional<std::vector<device::AggregatedEnumerateCredentialsResponse>>
        responses,
    base::Optional<size_t> remaining_credentials) {
  DCHECK_EQ(State::kGettingCredentials, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(credential_management_);
  DCHECK(!callback_id_.empty());

  if (status != device::CtapDeviceResponseCode::kSuccess) {
    OnFinished(device::FidoReturnCode::kAuthenticatorResponseInvalid);
    return;
  }
  DCHECK(responses);
  DCHECK(remaining_credentials);

  state_ = State::kReady;

  base::Value::ListStorage credentials;
  for (const auto& response : *responses) {
    for (const auto& credential : response.credentials) {
      base::DictionaryValue credential_value;
      std::string credential_id =
          base::HexEncode(credential.credential_id_cbor_bytes.data(),
                          credential.credential_id_cbor_bytes.size());
      if (credential_id.empty()) {
        NOTREACHED();
        continue;
      }
      credential_value.SetString("id", std::move(credential_id));
      credential_value.SetString("relyingPartyId", response.rp.id);
      credential_value.SetString("userName", credential.user.name.value_or(""));
      credential_value.SetString("userDisplayName",
                                 credential.user.display_name.value_or(""));
      credentials.emplace_back(std::move(credential_value));
    }
  }

  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::ListValue(std::move(credentials)));
}

void SecurityKeysCredentialHandler::OnGatherPIN(
    int64_t num_retries,
    base::OnceCallback<void(std::string)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback_id_.empty());
  DCHECK(!credential_management_provide_pin_cb_);

  credential_management_provide_pin_cb_ = std::move(callback);

  if (state_ == State::kStart) {
    // Resolve the promise to startCredentialManagement().
    state_ = State::kPIN;
    ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                              base::Value());
    return;
  }

  // Resolve the promise to credentialManagementProvidePIN().
  DCHECK_EQ(state_, State::kPIN);
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::Value(static_cast<int>(num_retries)));
}

void SecurityKeysCredentialHandler::OnCredentialsDeleted(
    device::CtapDeviceResponseCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(State::kDeletingCredentials, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(credential_management_);
  DCHECK(!callback_id_.empty());

  state_ = State::kReady;

  ResolveJavascriptCallback(
      base::Value(std::move(callback_id_)),
      base::Value(l10n_util::GetStringUTF8(
          status == device::CtapDeviceResponseCode::kSuccess
              ? IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_SUCCESS
              : IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_FAILED)));
}

void SecurityKeysCredentialHandler::OnFinished(device::FidoReturnCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int error;

  switch (status) {
    case device::FidoReturnCode::kSoftPINBlock:
      error = IDS_SETTINGS_SECURITY_KEYS_PIN_SOFT_LOCK;
      break;
    case device::FidoReturnCode::kHardPINBlock:
      error = IDS_SETTINGS_SECURITY_KEYS_PIN_HARD_LOCK;
      break;
    case device::FidoReturnCode::kAuthenticatorMissingCredentialManagement:
      error = IDS_SETTINGS_SECURITY_KEYS_NO_CREDENTIAL_MANAGEMENT;
      break;
    case device::FidoReturnCode::kAuthenticatorMissingUserVerification:
      error = IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_NO_PIN;
      break;
    case device::FidoReturnCode::kAuthenticatorResponseInvalid:
      error = IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_ERROR;
      break;
    case device::FidoReturnCode::kSuccess:
      error = IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_REMOVED;
      break;
    default:
      NOTREACHED();
      error = IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_ERROR;
      break;
  }

  FireWebUIListener("security-keys-credential-management-finished",
                    base::Value(l10n_util::GetStringUTF8(std::move(error))));
}

}  // namespace settings
