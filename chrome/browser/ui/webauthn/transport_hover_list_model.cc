// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"

#include <utility>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/webauthn/transport_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {
// The tag ID space consists of the union of |AuthenticatorTransport| values
// with the extra values defined below. These extra values must not overlap with
// any values of |AuthenticatorTransport|.

constexpr int kTagExtraBase = 1 << 16;
// Command ID for triggering QR flow.
constexpr int kPairPhoneTag = kTagExtraBase + 0;
constexpr int kNativeWinApiTag = kTagExtraBase + 1;
}  // namespace

TransportHoverListModel::TransportHoverListModel(
    std::vector<AuthenticatorTransport> transport_list,
    Delegate* delegate)
    : transport_list_(std::move(transport_list)), delegate_(delegate) {}

TransportHoverListModel::~TransportHoverListModel() = default;

bool TransportHoverListModel::ShouldShowPlaceholderForEmptyList() const {
  return false;
}

base::string16 TransportHoverListModel::GetPlaceholderText() const {
  return base::string16();
}

const gfx::VectorIcon* TransportHoverListModel::GetPlaceholderIcon() const {
  return &gfx::kNoneIcon;
}

std::vector<int> TransportHoverListModel::GetThrobberTags() const {
  return {};
}

std::vector<int> TransportHoverListModel::GetButtonTags() const {
  std::vector<int> tag_list(transport_list_.size());
  std::transform(
      transport_list_.begin(), transport_list_.end(), tag_list.begin(),
      [](const auto& transport) { return base::strict_cast<int>(transport); });
  return tag_list;
}

base::string16 TransportHoverListModel::GetItemText(int item_tag) const {
  return GetTransportHumanReadableName(
      static_cast<AuthenticatorTransport>(item_tag),
      TransportSelectionContext::kTransportSelectionSheet);
}

base::string16 TransportHoverListModel::GetDescriptionText(int item_tag) const {
  return base::string16();
}

const gfx::VectorIcon* TransportHoverListModel::GetItemIcon(
    int item_tag) const {
  return GetTransportVectorIcon(static_cast<AuthenticatorTransport>(item_tag));
}

void TransportHoverListModel::OnListItemSelected(int item_tag) {
  if (!delegate_) {
    return;
  }

  delegate_->OnTransportSelected(static_cast<AuthenticatorTransport>(item_tag));
}

size_t TransportHoverListModel::GetPreferredItemCount() const {
  return transport_list_.size();
}

bool TransportHoverListModel::StyleForTwoLines() const {
  return false;
}

TransportHoverListModel2::TransportHoverListModel2(
    std::vector<AuthenticatorTransport> transport_list,
    bool cable_extension_provided,
    bool win_native_api_enabled,
    Delegate* delegate)
    : transport_list_(std::move(transport_list)),
      cable_extension_provided_(cable_extension_provided),
      win_native_api_enabled_(win_native_api_enabled),
      delegate_(delegate) {}

TransportHoverListModel2::~TransportHoverListModel2() = default;

bool TransportHoverListModel2::ShouldShowPlaceholderForEmptyList() const {
  return false;
}

base::string16 TransportHoverListModel2::GetPlaceholderText() const {
  return base::string16();
}

const gfx::VectorIcon* TransportHoverListModel2::GetPlaceholderIcon() const {
  return &gfx::kNoneIcon;
}

std::vector<int> TransportHoverListModel2::GetThrobberTags() const {
  std::vector<int> tags;

  if (base::Contains(
          transport_list_,
          AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy)) {
    tags.push_back(base::strict_cast<int>(
        AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy));
  }

  if (base::Contains(transport_list_,
                     AuthenticatorTransport::kUsbHumanInterfaceDevice)) {
    tags.push_back(base::strict_cast<int>(
        AuthenticatorTransport::kUsbHumanInterfaceDevice));
  }

  return tags;
}

std::vector<int> TransportHoverListModel2::GetButtonTags() const {
  std::vector<int> tags({kPairPhoneTag});

  if (win_native_api_enabled_) {
    tags.push_back(kNativeWinApiTag);
  }

  for (const auto transport : transport_list_) {
    switch (transport) {
      // These are throbbers, not buttons; ignore.
      case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      case AuthenticatorTransport::kUsbHumanInterfaceDevice:
        break;

      case AuthenticatorTransport::kInternal:
        tags.push_back(base::strict_cast<int>(transport));
        break;

      default:
        DCHECK(false) << static_cast<int>(transport);
    }
  }

  return tags;
}

base::string16 TransportHoverListModel2::GetItemText(int item_tag) const {
  switch (item_tag) {
    case kPairPhoneTag:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_TRANSPORT_POPUP_PAIR_PHONE);

    case kNativeWinApiTag:
      return l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_TRANSPORT_POPUP_DIFFERENT_AUTHENTICATOR_WIN);

    default:
      return GetTransportHumanReadableName(
          static_cast<AuthenticatorTransport>(item_tag),
          TransportSelectionContext::kTransportSelectionSheet);
  }
}

base::string16 TransportHoverListModel2::GetDescriptionText(
    int item_tag) const {
  switch (item_tag) {
    case base::strict_cast<int>(
        AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy):
      if (cable_extension_provided_) {
        return l10n_util::GetStringUTF16(
            IDS_WEBAUTHN_CABLE_ACTIVATE_DESCRIPTION_SHORT);
      }
      return l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_CABLE_V2_ACTIVATE_DESCRIPTION_SHORT);

    case base::strict_cast<int>(
        AuthenticatorTransport::kUsbHumanInterfaceDevice):
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_USB_ACTIVATE_DESCRIPTION);

    default:
      return base::string16();
  }
}

const gfx::VectorIcon* TransportHoverListModel2::GetItemIcon(
    int item_tag) const {
  switch (item_tag) {
    case kPairPhoneTag:
      return &kSmartphoneIcon;

    case kNativeWinApiTag:
      return GetTransportVectorIcon(
          AuthenticatorTransport::kUsbHumanInterfaceDevice);

    default:
      return GetTransportVectorIcon(
          static_cast<AuthenticatorTransport>(item_tag));
  }
}

void TransportHoverListModel2::OnListItemSelected(int item_tag) {
  if (!delegate_) {
    return;
  }

  switch (item_tag) {
    case kPairPhoneTag:
      delegate_->StartPhonePairing();
      break;

    case kNativeWinApiTag:
      delegate_->StartWinNativeApi();
      break;

    default:
      delegate_->OnTransportSelected(
          static_cast<AuthenticatorTransport>(item_tag));
      break;
  }
}

size_t TransportHoverListModel2::GetPreferredItemCount() const {
  return transport_list_.size() + /* pairing a phone */ 1 +
         static_cast<int>(win_native_api_enabled_);
}

bool TransportHoverListModel2::StyleForTwoLines() const {
  return true;
}
