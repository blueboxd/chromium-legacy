// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/settings/settings_security_key_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "content/public/browser/web_ui.h"
#include "device/fido/pin.h"
#include "device/fido/reset_request_handler.h"
#include "device/fido/set_pin_request_handler.h"

using content::BrowserThread;

namespace {

constexpr char kStartSetPIN[] = "securityKeyStartSetPIN";
constexpr char kSetPIN[] = "securityKeySetPIN";
constexpr char kClose[] = "securityKeyClose";
constexpr char kReset[] = "securityKeyReset";
constexpr char kCompleteReset[] = "securityKeyCompleteReset";

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
      kStartSetPIN, base::BindRepeating(&SecurityKeysHandler::HandleStartSetPIN,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kSetPIN, base::BindRepeating(&SecurityKeysHandler::HandleSetPIN,
                                   base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kClose, base::BindRepeating(&SecurityKeysHandler::HandleClose,
                                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kReset, base::BindRepeating(&SecurityKeysHandler::HandleReset,
                                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kCompleteReset,
      base::BindRepeating(&SecurityKeysHandler::HandleCompleteReset,
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
  callback_id_.clear();
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

void SecurityKeysHandler::HandleClose(const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());
  Close();
}

}  // namespace settings
