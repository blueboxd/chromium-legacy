// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"

#if defined(OS_MACOSX)
#include "device/fido/mac/authenticator.h"
#include "device/fido/mac/credential_metadata.h"
#endif

#if defined(OS_WIN)
#include "device/fido/win/authenticator.h"
#endif

namespace {

// Returns true iff |relying_party_id| is listed in the
// SecurityKeyPermitAttestation policy.
bool IsWebauthnRPIDListedInEnterprisePolicy(
    content::BrowserContext* browser_context,
    const std::string& relying_party_id) {
#if defined(OS_ANDROID)
  return false;
#else
  const Profile* profile = Profile::FromBrowserContext(browser_context);
  const PrefService* prefs = profile->GetPrefs();
  const base::ListValue* permit_attestation =
      prefs->GetList(prefs::kSecurityKeyPermitAttestation);
  return std::any_of(permit_attestation->begin(), permit_attestation->end(),
                     [&relying_party_id](const base::Value& v) {
                       return v.GetString() == relying_party_id;
                     });
#endif
}

}  // namespace

#if defined(OS_MACOSX)
static const char kWebAuthnTouchIdMetadataSecretPrefName[] =
    "webauthn.touchid.metadata_secret";
#endif

static const char kWebAuthnLastTransportUsedPrefName[] =
    "webauthn.last_transport_used";

static const char kWebAuthnBlePairedMacAddressesPrefName[] =
    "webauthn.ble.paired_mac_addresses";

// static
void ChromeAuthenticatorRequestDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(OS_MACOSX)
  registry->RegisterStringPref(kWebAuthnTouchIdMetadataSecretPrefName,
                               std::string());
#endif

  registry->RegisterStringPref(kWebAuthnLastTransportUsedPrefName,
                               std::string());
  registry->RegisterListPref(kWebAuthnBlePairedMacAddressesPrefName);
}

ChromeAuthenticatorRequestDelegate::ChromeAuthenticatorRequestDelegate(
    content::RenderFrameHost* render_frame_host,
    const std::string& relying_party_id)
    : render_frame_host_(render_frame_host),
      relying_party_id_(relying_party_id),
      transient_dialog_model_holder_(
          std::make_unique<AuthenticatorRequestDialogModel>(relying_party_id)),
      weak_dialog_model_(transient_dialog_model_holder_.get()) {}

ChromeAuthenticatorRequestDelegate::~ChromeAuthenticatorRequestDelegate() {
  // Currently, completion of the request is indicated by //content destroying
  // this delegate.
  if (weak_dialog_model_) {
    weak_dialog_model_->OnRequestComplete();
  }

  // The dialog model may be destroyed after the OnRequestComplete call.
  if (weak_dialog_model_) {
    weak_dialog_model_->RemoveObserver(this);
    weak_dialog_model_ = nullptr;
  }
}

base::WeakPtr<ChromeAuthenticatorRequestDelegate>
ChromeAuthenticatorRequestDelegate::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

AuthenticatorRequestDialogModel*
ChromeAuthenticatorRequestDelegate::WeakDialogModelForTesting() const {
  return weak_dialog_model_;
}

content::BrowserContext* ChromeAuthenticatorRequestDelegate::browser_context()
    const {
  return content::WebContents::FromRenderFrameHost(render_frame_host())
      ->GetBrowserContext();
}

bool ChromeAuthenticatorRequestDelegate::DoesBlockRequestOnFailure(
    const ::device::FidoAuthenticator* authenticator,
    InterestingFailureReason reason) {
  if (!IsWebAuthnUIEnabled())
    return false;
  if (!weak_dialog_model_)
    return false;

  DCHECK(authenticator || reason == InterestingFailureReason::kTimeout);

#if defined(OS_WIN)
  if (authenticator && authenticator->IsWinNativeApiAuthenticator()) {
    // Do not display a Chrome error dialog if the user cancels out of the
    // Windows UI. No other errors are reachable.
    DCHECK(reason == InterestingFailureReason::kUserConsentDenied);
    return false;
  }
#endif  // defined(OS_WIN)

  switch (reason) {
    case InterestingFailureReason::kTimeout:
      weak_dialog_model_->OnRequestTimeout();
      break;
    case InterestingFailureReason::kKeyNotRegistered:
      weak_dialog_model_->OnActivatedKeyNotRegistered();
      break;
    case InterestingFailureReason::kKeyAlreadyRegistered:
      weak_dialog_model_->OnActivatedKeyAlreadyRegistered();
      break;
    case InterestingFailureReason::kSoftPINBlock:
      weak_dialog_model_->OnSoftPINBlock();
      break;
    case InterestingFailureReason::kHardPINBlock:
      weak_dialog_model_->OnHardPINBlock();
      break;
    case InterestingFailureReason::kAuthenticatorRemovedDuringPINEntry:
      weak_dialog_model_->OnAuthenticatorRemovedDuringPINEntry();
      break;
    case InterestingFailureReason::kAuthenticatorMissingResidentKeys:
      weak_dialog_model_->OnAuthenticatorMissingResidentKeys();
      break;
    case InterestingFailureReason::kAuthenticatorMissingUserVerification:
      weak_dialog_model_->OnAuthenticatorMissingUserVerification();
      break;
    case InterestingFailureReason::kStorageFull:
      weak_dialog_model_->OnAuthenticatorStorageFull();
      break;
    case InterestingFailureReason::kUserConsentDenied:
      weak_dialog_model_->OnUserConsentDenied();
      break;
  }
  return true;
}

void ChromeAuthenticatorRequestDelegate::RegisterActionCallbacks(
    base::OnceClosure cancel_callback,
    base::Closure start_over_callback,
    device::FidoRequestHandlerBase::RequestCallback request_callback,
    base::RepeatingClosure bluetooth_adapter_power_on_callback,
    device::FidoRequestHandlerBase::BlePairingCallback ble_pairing_callback) {
  request_callback_ = request_callback;
  cancel_callback_ = std::move(cancel_callback);
  start_over_callback_ = std::move(start_over_callback);

  weak_dialog_model_->SetRequestCallback(request_callback);
  weak_dialog_model_->SetBluetoothAdapterPowerOnCallback(
      bluetooth_adapter_power_on_callback);
  weak_dialog_model_->SetBlePairingCallback(ble_pairing_callback);
  weak_dialog_model_->SetBleDevicePairedCallback(base::BindRepeating(
      &ChromeAuthenticatorRequestDelegate::AddFidoBleDeviceToPairedList,
      weak_ptr_factory_.GetWeakPtr()));
}

bool ChromeAuthenticatorRequestDelegate::ShouldPermitIndividualAttestation(
    const std::string& relying_party_id) {
  constexpr char kGoogleCorpAppId[] =
      "https://www.gstatic.com/securitykey/a/google.com/origins.json";

  // If the RP ID is actually the Google corp App ID (because the request is
  // actually a U2F request originating from cryptotoken), or is listed in the
  // enterprise policy, signal that individual attestation is permitted.
  return relying_party_id == kGoogleCorpAppId ||
         IsWebauthnRPIDListedInEnterprisePolicy(browser_context(),
                                                relying_party_id);
}

void ChromeAuthenticatorRequestDelegate::ShouldReturnAttestation(
    const std::string& relying_party_id,
    const device::FidoAuthenticator* authenticator,
    base::OnceCallback<void(bool)> callback) {
#if defined(OS_ANDROID)
  // Android is expected to use platform APIs for webauthn which will take care
  // of prompting.
  std::move(callback).Run(true);
#else
  if (IsWebauthnRPIDListedInEnterprisePolicy(browser_context(),
                                             relying_party_id)) {
    std::move(callback).Run(true);
    return;
  }

  // Cryptotoken displays its own attestation consent prompt.
  // AuthenticatorCommon does not invoke ShouldReturnAttestation() for those
  // requests.
  if (disable_ui_) {
    NOTREACHED();
    std::move(callback).Run(false);
    return;
  }

#if defined(OS_WIN)
  if (authenticator->IsWinNativeApiAuthenticator() &&
      static_cast<const device::WinWebAuthnApiAuthenticator*>(authenticator)
          ->ShowsPrivacyNotice()) {
    // The OS' native API includes an attestation prompt.
    std::move(callback).Run(true);
    return;
  }
#endif  // defined(OS_WIN)

  weak_dialog_model_->RequestAttestationPermission(std::move(callback));
#endif
}

bool ChromeAuthenticatorRequestDelegate::SupportsResidentKeys() {
  return true;
}

void ChromeAuthenticatorRequestDelegate::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  if (disable_ui_) {
    // Cryptotoken requests should never reach account selection.
    NOTREACHED();
    std::move(cancel_callback_).Run();
    return;
  }

  if (!weak_dialog_model_) {
    std::move(cancel_callback_).Run();
    return;
  }

  weak_dialog_model_->SelectAccount(std::move(responses), std::move(callback));
}

bool ChromeAuthenticatorRequestDelegate::IsFocused() {
#if defined(OS_ANDROID)
  // Android is expected to use platform APIs for webauthn.
  return true;
#else
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host());
  DCHECK(web_contents);
  return web_contents->GetVisibility() == content::Visibility::VISIBLE;
#endif
}

#if defined(OS_MACOSX)
static constexpr char kTouchIdKeychainAccessGroup[] =
    "EQHXZ8M8AV.com.google.Chrome.webauthn";

namespace {

std::string TouchIdMetadataSecret(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  std::string key = prefs->GetString(kWebAuthnTouchIdMetadataSecretPrefName);
  if (key.empty() || !base::Base64Decode(key, &key)) {
    key = device::fido::mac::GenerateCredentialMetadataSecret();
    std::string encoded_key;
    base::Base64Encode(key, &encoded_key);
    prefs->SetString(kWebAuthnTouchIdMetadataSecretPrefName, encoded_key);
  }
  return key;
}

}  // namespace

// static
ChromeAuthenticatorRequestDelegate::TouchIdAuthenticatorConfig
ChromeAuthenticatorRequestDelegate::TouchIdAuthenticatorConfigForProfile(
    Profile* profile) {
  return TouchIdAuthenticatorConfig{kTouchIdKeychainAccessGroup,
                                    TouchIdMetadataSecret(profile)};
}
#endif

void ChromeAuthenticatorRequestDelegate::UpdateLastTransportUsed(
    device::FidoTransportProtocol transport) {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  prefs->SetString(kWebAuthnLastTransportUsedPrefName,
                   device::ToString(transport));

  if (!weak_dialog_model_)
    return;

  // We already invoke AddFidoBleDeviceToPairedList() on
  // AuthenticatorRequestDialogModel::OnPairingSuccess(). We invoke the function
  // here once more to take into account the case when user pairs Bluetooth
  // authenticator separately via system OS rather than using Chrome WebAuthn
  // UI. AddFidoBleDeviceToPairedList() handles the case when duplicate
  // authenticator id is being stored.
  const auto& selected_bluetooth_authenticator_id =
      weak_dialog_model_->selected_authenticator_id();
  if (transport == device::FidoTransportProtocol::kBluetoothLowEnergy &&
      selected_bluetooth_authenticator_id) {
    AddFidoBleDeviceToPairedList(*selected_bluetooth_authenticator_id);
  }
}

void ChromeAuthenticatorRequestDelegate::DisableUI() {
  disable_ui_ = true;
}

bool ChromeAuthenticatorRequestDelegate::IsWebAuthnUIEnabled() {
  // The UI is fully disabled for the entire request duration only if the
  // request originates from cryptotoken. The UI may be hidden in other
  // circumstances (e.g. while showing the native Windows WebAuthn UI). But in
  // those cases the UI is still enabled and can be shown e.g. for an
  // attestation consent prompt.
  return !disable_ui_;
}

bool ChromeAuthenticatorRequestDelegate::
    IsUserVerifyingPlatformAuthenticatorAvailable() {
#if defined(OS_MACOSX)
  // Touch ID is available in Incognito, but not in Guest mode.
  if (Profile::FromBrowserContext(browser_context())->IsGuestSession())
    return false;

  return device::fido::mac::TouchIdAuthenticator::IsAvailable(
      TouchIdAuthenticatorConfigForProfile(
          Profile::FromBrowserContext(browser_context())));
#elif defined(OS_WIN)
  if (browser_context()->IsOffTheRecord())
    return false;

  return base::FeatureList::IsEnabled(device::kWebAuthUseNativeWinApi) &&
         device::WinWebAuthnApiAuthenticator::
             IsUserVerifyingPlatformAuthenticatorAvailable();
#else
  return false;
#endif  // defined(OS_MACOSX) || defined(OS_WIN)
}

#if defined(OS_MACOSX)
base::Optional<ChromeAuthenticatorRequestDelegate::TouchIdAuthenticatorConfig>
ChromeAuthenticatorRequestDelegate::GetTouchIdAuthenticatorConfig() {
  if (!IsUserVerifyingPlatformAuthenticatorAvailable())
    return base::nullopt;

  return TouchIdAuthenticatorConfigForProfile(
      Profile::FromBrowserContext(browser_context()));
}
#endif  // defined(OS_MACOSX)

void ChromeAuthenticatorRequestDelegate::OnTransportAvailabilityEnumerated(
    device::FidoRequestHandlerBase::TransportAvailabilityInfo data) {
#if !defined(OS_ANDROID)
  if (disable_ui_ || !transient_dialog_model_holder_) {
    return;
  }

  weak_dialog_model_->AddObserver(this);
  weak_dialog_model_->set_incognito_mode(
      Profile::FromBrowserContext(browser_context())->IsIncognitoProfile());

  weak_dialog_model_->StartFlow(std::move(data), GetLastTransportUsed(),
                                GetPreviouslyPairedFidoBleDeviceIds());

  ShowAuthenticatorRequestDialog(
      content::WebContents::FromRenderFrameHost(render_frame_host()),
      std::move(transient_dialog_model_holder_));
#endif  // !defined(OS_ANDROID)
}

bool ChromeAuthenticatorRequestDelegate::EmbedderControlsAuthenticatorDispatch(
    const device::FidoAuthenticator& authenticator) {
  // Decide whether the //device/fido code should dispatch the current
  // request to an authenticator immediately after it has been
  // discovered, or whether the embedder/UI takes charge of that by
  // invoking its RequestCallback.
  auto transport = authenticator.AuthenticatorTransport();
  return IsWebAuthnUIEnabled() &&
         (!transport ||  // Windows
          *transport == device::FidoTransportProtocol::kInternal ||
          *transport == device::FidoTransportProtocol::kBluetoothLowEnergy);
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorAdded(
    const device::FidoAuthenticator& authenticator) {
  if (!IsWebAuthnUIEnabled())
    return;

  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->AddAuthenticator(authenticator);
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorRemoved(
    base::StringPiece authenticator_id) {
  if (!IsWebAuthnUIEnabled())
    return;

  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->RemoveAuthenticator(authenticator_id);
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorIdChanged(
    base::StringPiece old_authenticator_id,
    std::string new_authenticator_id) {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->UpdateAuthenticatorReferenceId(
      old_authenticator_id, std::move(new_authenticator_id));
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorPairingModeChanged(
    base::StringPiece authenticator_id,
    bool is_in_pairing_mode,
    base::string16 display_name) {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->UpdateAuthenticatorReferencePairingMode(
      authenticator_id, is_in_pairing_mode, display_name);
}

void ChromeAuthenticatorRequestDelegate::BluetoothAdapterPowerChanged(
    bool is_powered_on) {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->OnBluetoothPoweredStateChanged(is_powered_on);
}

bool ChromeAuthenticatorRequestDelegate::SupportsPIN() const {
  return true;
}

void ChromeAuthenticatorRequestDelegate::CollectPIN(
    base::Optional<int> attempts,
    base::OnceCallback<void(std::string)> provide_pin_cb) {
  if (!weak_dialog_model_)
    return;

  weak_dialog_model_->CollectPIN(attempts, std::move(provide_pin_cb));
}

void ChromeAuthenticatorRequestDelegate::FinishCollectPIN() {
  weak_dialog_model_->SetCurrentStep(
      AuthenticatorRequestDialogModel::Step::kClientPinTapAgain);
}

void ChromeAuthenticatorRequestDelegate::SetMightCreateResidentCredential(
    bool v) {
  if (!weak_dialog_model_) {
    return;
  }
  weak_dialog_model_->set_might_create_resident_credential(v);
}

void ChromeAuthenticatorRequestDelegate::OnStartOver() {
  DCHECK(start_over_callback_);
  start_over_callback_.Run();
}

void ChromeAuthenticatorRequestDelegate::OnModelDestroyed() {
  DCHECK(weak_dialog_model_);
  weak_dialog_model_ = nullptr;
}

void ChromeAuthenticatorRequestDelegate::OnCancelRequest() {
  // |cancel_callback_| must be invoked at most once as invocation of
  // |cancel_callback_| will destroy |this|.
  DCHECK(cancel_callback_);
  std::move(cancel_callback_).Run();
}

void ChromeAuthenticatorRequestDelegate::AddFidoBleDeviceToPairedList(
    std::string ble_authenticator_id) {
  ListPrefUpdate update(
      Profile::FromBrowserContext(browser_context())->GetPrefs(),
      kWebAuthnBlePairedMacAddressesPrefName);
  bool already_contains_address = std::any_of(
      update->begin(), update->end(),
      [&ble_authenticator_id](const auto& value) {
        return value.is_string() && value.GetString() == ble_authenticator_id;
      });

  if (already_contains_address)
    return;

  update->Append(
      std::make_unique<base::Value>(std::move(ble_authenticator_id)));
}

base::Optional<device::FidoTransportProtocol>
ChromeAuthenticatorRequestDelegate::GetLastTransportUsed() const {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  return device::ConvertToFidoTransportProtocol(
      prefs->GetString(kWebAuthnLastTransportUsedPrefName));
}

const base::ListValue*
ChromeAuthenticatorRequestDelegate::GetPreviouslyPairedFidoBleDeviceIds()
    const {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  return prefs->GetList(kWebAuthnBlePairedMacAddressesPrefName);
}
