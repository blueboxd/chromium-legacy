// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_security_key_handler.h"

#include <utility>

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

}  // namespace

namespace settings {

SecurityKeysHandler::SecurityKeysHandler()
    : state_(State::kNone),
      weak_factory_(new base::WeakPtrFactory<SecurityKeysHandler>(this)) {}

SecurityKeysHandler::~SecurityKeysHandler() = default;

void SecurityKeysHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "securityKeyStartSetPIN",
      base::BindRepeating(&SecurityKeysHandler::HandleStartSetPIN,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeySetPIN",
      base::BindRepeating(&SecurityKeysHandler::HandleSetPIN,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyClose", base::BindRepeating(&SecurityKeysHandler::HandleClose,
                                              base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyReset", base::BindRepeating(&SecurityKeysHandler::HandleReset,
                                              base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCompleteReset",
      base::BindRepeating(&SecurityKeysHandler::HandleCompleteReset,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagement",
      base::BindRepeating(&SecurityKeysHandler::HandleCredentialManagement,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementPIN",
      base::BindRepeating(&SecurityKeysHandler::HandleCredentialManagementPIN,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityKeyCredentialManagementEnumerate",
      base::BindRepeating(
          &SecurityKeysHandler::HandleCredentialManagementEnumerate,
          base::Unretained(this)));
}

void SecurityKeysHandler::OnJavascriptAllowed() {}
void SecurityKeysHandler::OnJavascriptDisallowed() {
  // If Javascript is disallowed, |Close| will invalidate all current WeakPtrs
  // and thus drop all pending callbacks. This means that
  // |IsJavascriptAllowed| doesn't need to be tested before each callback
  // because, if the callback into this object happened, then Javascript is
  // allowed.
  Close();
}

void SecurityKeysHandler::Close() {
  // Invalidate all existing WeakPtrs so that no stale callbacks occur.
  weak_factory_ =
      std::make_unique<base::WeakPtrFactory<SecurityKeysHandler>>(this);
  state_ = State::kNone;
  set_pin_.reset();
  reset_.reset();
  discovery_factory_.reset();
  credential_management_.reset();
  callback_id_.clear();
  credential_management_provide_pin_cb_.Reset();
  DCHECK(!credential_management_provide_pin_cb_);
}

void SecurityKeysHandler::HandleStartSetPIN(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(State::kNone, state_);
  DCHECK_EQ(1u, args->GetSize());

  AllowJavascript();
  DCHECK(callback_id_.empty());
  callback_id_ = args->GetList()[0].GetString();
  state_ = State::kStartSetPIN;
  set_pin_ = std::make_unique<device::SetPINRequestHandler>(
      content::GetSystemConnector(), supported_transports(),
      base::BindOnce(&SecurityKeysHandler::OnGatherPIN,
                     weak_factory_->GetWeakPtr()),
      base::BindRepeating(&SecurityKeysHandler::OnSetPINComplete,
                          weak_factory_->GetWeakPtr()));
}

void SecurityKeysHandler::OnGatherPIN(base::Optional<int64_t> num_retries) {
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

void SecurityKeysHandler::OnSetPINComplete(
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

void SecurityKeysHandler::HandleSetPIN(const base::ListValue* args) {
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

void SecurityKeysHandler::HandleReset(const base::ListValue* args) {
  DCHECK_EQ(State::kNone, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args->GetSize());

  AllowJavascript();
  DCHECK(callback_id_.empty());
  callback_id_ = args->GetList()[0].GetString();

  state_ = State::kStartReset;
  reset_ = std::make_unique<device::ResetRequestHandler>(
      content::GetSystemConnector(), supported_transports(),
      base::BindOnce(&SecurityKeysHandler::OnResetSent,
                     weak_factory_->GetWeakPtr()),
      base::BindOnce(&SecurityKeysHandler::OnResetFinished,
                     weak_factory_->GetWeakPtr()));
}

void SecurityKeysHandler::OnResetSent() {
  DCHECK_EQ(State::kStartReset, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // A reset message has been sent to a security key and it may complete
  // before Javascript asks for the result. Therefore |HandleCompleteReset|
  // and |OnResetFinished| may be called in either order.
  state_ = State::kWaitingForResetNoCallbackYet;
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::Value(0 /* success */));
}

void SecurityKeysHandler::HandleCompleteReset(const base::ListValue* args) {
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

void SecurityKeysHandler::OnResetFinished(
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

void SecurityKeysHandler::HandleCredentialManagement(
    const base::ListValue* args) {
  DCHECK_EQ(State::kNone, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args->GetSize());
  DCHECK(!credential_management_);
  DCHECK(!discovery_factory_);

  AllowJavascript();
  DCHECK(callback_id_.empty());
  callback_id_ = args->GetList()[0].GetString();

  state_ = State::kCredentialManagementStart;
  discovery_factory_ = std::make_unique<device::FidoDiscoveryFactory>();
  credential_management_ =
      std::make_unique<device::CredentialManagementHandler>(
          content::ServiceManagerConnection::GetForProcess()->GetConnector(),
          discovery_factory_.get(), supported_transports(),
          base::BindOnce(&SecurityKeysHandler::OnCredentialManagementReady,
                         weak_factory_->GetWeakPtr()),
          base::BindRepeating(
              &SecurityKeysHandler::OnCredentialManagementGatherPIN,
              weak_factory_->GetWeakPtr()),
          base::BindOnce(&SecurityKeysHandler::OnCredentialManagementFinished,
                         weak_factory_->GetWeakPtr()));
}

void SecurityKeysHandler::HandleCredentialManagementPIN(
    const base::ListValue* args) {
  DCHECK_EQ(State::kCredentialManagementPIN, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(2u, args->GetSize());
  DCHECK(credential_management_);
  DCHECK(credential_management_provide_pin_cb_);
  DCHECK(callback_id_.empty());

  callback_id_ = args->GetList()[0].GetString();
  std::string pin = args->GetList()[1].GetString();

  std::move(credential_management_provide_pin_cb_).Run(pin);
}

void SecurityKeysHandler::HandleCredentialManagementEnumerate(
    const base::ListValue* args) {
  DCHECK_EQ(state_, State::kCredentialManagementReady);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(1u, args->GetSize());
  DCHECK(credential_management_);
  DCHECK(callback_id_.empty());

  state_ = State::kCredentialManagementGettingCredentials;
  callback_id_ = args->GetList()[0].GetString();
  credential_management_->GetCredentials(base::BindOnce(
      &SecurityKeysHandler::OnHaveCredentials, weak_factory_->GetWeakPtr()));
}

void SecurityKeysHandler::OnCredentialManagementReady() {
  DCHECK(state_ == State::kCredentialManagementStart ||
         state_ == State::kCredentialManagementPIN);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(credential_management_);
  DCHECK(!callback_id_.empty());

  state_ = State::kCredentialManagementReady;
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::Value());
}

void SecurityKeysHandler::OnHaveCredentials(
    device::CtapDeviceResponseCode status,
    base::Optional<std::vector<device::AggregatedEnumerateCredentialsResponse>>
        responses,
    base::Optional<size_t> remaining_credentials) {
  DCHECK_EQ(State::kCredentialManagementGettingCredentials, state_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(credential_management_);
  DCHECK(!callback_id_.empty());

  if (status != device::CtapDeviceResponseCode::kSuccess) {
    OnCredentialManagementFinished(
        device::FidoReturnCode::kAuthenticatorResponseInvalid);
    return;
  }
  DCHECK(responses);
  DCHECK(remaining_credentials);

  state_ = State::kCredentialManagementReady;

  base::Value::ListStorage credentials;
  if (responses) {
    for (const auto& response : *responses) {
      for (const auto& credential : response.credentials) {
        base::DictionaryValue credential_value;
        credential_value.SetString(
            "id", base::HexEncode(credential.credential_id.id().data(),
                                  credential.credential_id.id().size()));
        credential_value.SetString("relyingPartyId", response.rp.id);
        credential_value.SetString("userName",
                                   credential.user.name.value_or(""));
        credential_value.SetString("userDisplayName",
                                   credential.user.display_name.value_or(""));
        credentials.emplace_back(std::move(credential_value));
      }
    }
  }

  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::ListValue(std::move(credentials)));
}

void SecurityKeysHandler::OnCredentialManagementGatherPIN(
    int64_t num_retries,
    base::OnceCallback<void(std::string)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback_id_.empty());
  DCHECK(!credential_management_provide_pin_cb_);

  credential_management_provide_pin_cb_ = std::move(callback);

  if (state_ == State::kCredentialManagementStart) {
    // Resolve the promise to startCredentialManagement().
    state_ = State::kCredentialManagementPIN;
    ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                              base::Value());
    return;
  }

  // Resolve the promise to credentialManagementProvidePIN().
  DCHECK_EQ(state_, State::kCredentialManagementPIN);
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)),
                            base::Value(static_cast<int>(num_retries)));
}

void SecurityKeysHandler::OnCredentialManagementFinished(
    device::FidoReturnCode status) {
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

void SecurityKeysHandler::HandleClose(const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());
  Close();
}

}  // namespace settings
