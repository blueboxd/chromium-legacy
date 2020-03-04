// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

#if !defined(OS_ANDROID)
#include "chrome/app/vector_icons/vector_icons.h"
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

namespace autofill {

namespace {

#if !defined(OS_ANDROID)
// Default sice for icons in the autofill popup.
constexpr int kIconSize = 16;
#endif

// Used in the IDS_ space as a placeholder for resources that don't exist.
constexpr int kResourceNotFoundId = 0;

const struct {
  const char* name;
  int icon_id;
  int accessible_string_id;
} kDataResources[] = {
    {autofill::kAmericanExpressCard, IDR_AUTOFILL_CC_AMEX,
     IDS_AUTOFILL_CC_AMEX},
    {autofill::kDinersCard, IDR_AUTOFILL_CC_DINERS, IDS_AUTOFILL_CC_DINERS},
    {autofill::kDiscoverCard, IDR_AUTOFILL_CC_DISCOVER,
     IDS_AUTOFILL_CC_DISCOVER},
    {autofill::kEloCard, IDR_AUTOFILL_CC_ELO, IDS_AUTOFILL_CC_ELO},
    {autofill::kGenericCard, IDR_AUTOFILL_CC_GENERIC, kResourceNotFoundId},
    {autofill::kJCBCard, IDR_AUTOFILL_CC_JCB, IDS_AUTOFILL_CC_JCB},
    {autofill::kMasterCard, IDR_AUTOFILL_CC_MASTERCARD,
     IDS_AUTOFILL_CC_MASTERCARD},
    {autofill::kMirCard, IDR_AUTOFILL_CC_MIR, IDS_AUTOFILL_CC_MIR},
    {autofill::kUnionPay, IDR_AUTOFILL_CC_UNIONPAY, IDS_AUTOFILL_CC_UNION_PAY},
    {autofill::kVisaCard, IDR_AUTOFILL_CC_VISA, IDS_AUTOFILL_CC_VISA},
#if defined(OS_ANDROID)
    {"httpWarning", IDR_ANDROID_AUTOFILL_HTTP_WARNING, kResourceNotFoundId},
    {"httpsInvalid", IDR_ANDROID_AUTOFILL_HTTPS_INVALID_WARNING,
     kResourceNotFoundId},
    {"scanCreditCardIcon", IDR_ANDROID_AUTOFILL_CC_SCAN_NEW,
     kResourceNotFoundId},
    {"settings", IDR_ANDROID_AUTOFILL_SETTINGS, kResourceNotFoundId},
    {"create", IDR_ANDROID_AUTOFILL_CREATE, kResourceNotFoundId},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"googlePay", IDR_ANDROID_AUTOFILL_GOOGLE_PAY, kResourceNotFoundId},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"googlePay", IDR_AUTOFILL_GOOGLE_PAY, kResourceNotFoundId},
    {"googlePayDark", IDR_AUTOFILL_GOOGLE_PAY_DARK, kResourceNotFoundId},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
};

}  // namespace

AutofillPopupLayoutModel::AutofillPopupLayoutModel(bool is_credit_card_popup)
    : is_credit_card_popup_(is_credit_card_popup) {}

AutofillPopupLayoutModel::~AutofillPopupLayoutModel() = default;

#if !defined(OS_ANDROID)
// static
gfx::ImageSkia AutofillPopupLayoutModel::GetIconImage(
    const autofill::Suggestion& suggestion) {
  if (!suggestion.custom_icon.IsEmpty())
    return suggestion.custom_icon.AsImageSkia();

  return GetIconImageByName(suggestion.icon);
}

// static
gfx::ImageSkia AutofillPopupLayoutModel::GetStoreIndicatorIconImage(
    const autofill::Suggestion& suggestion) {
  return GetIconImageByName(suggestion.store_indicator_icon);
}
#endif  // !defined(OS_ANDROID)

// static
int AutofillPopupLayoutModel::GetIconResourceID(
    const std::string& resource_name) {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (resource_name == "googlePay" || resource_name == "googlePayDark") {
    return 0;
  }
#endif
  int result = kResourceNotFoundId;
  for (size_t i = 0; i < base::size(kDataResources); ++i) {
    if (resource_name == kDataResources[i].name) {
      result = kDataResources[i].icon_id;
      break;
    }
  }

  return result;
}

void AutofillPopupLayoutModel::SetUpForTesting(
    std::unique_ptr<PopupViewCommon> view_common) {
  view_common_ = std::move(view_common);
}

#if !defined(OS_ANDROID)
// static
gfx::ImageSkia AutofillPopupLayoutModel::GetIconImageByName(
    const std::string& icon_str) {
  if (icon_str.empty())
    return gfx::ImageSkia();

  // For http warning message, get icon images from VectorIcon, which is the
  // same as security indicator icons in location bar.
  if (icon_str == "httpWarning") {
    return gfx::CreateVectorIcon(omnibox::kHttpIcon, kIconSize,
                                 gfx::kChromeIconGrey);
  }
  if (icon_str == "httpsInvalid") {
    return gfx::CreateVectorIcon(omnibox::kNotSecureWarningIcon, kIconSize,
                                 gfx::kGoogleRed700);
  }
  if (icon_str == "keyIcon") {
    return gfx::CreateVectorIcon(kKeyIcon, kIconSize, gfx::kChromeIconGrey);
  }
  if (icon_str == "globeIcon") {
    return gfx::CreateVectorIcon(kGlobeIcon, kIconSize, gfx::kChromeIconGrey);
  }
  if (icon_str == "google") {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    return gfx::CreateVectorIcon(kGoogleGLogoIcon, kIconSize,
                                 gfx::kPlaceholderColor);
#else
    return gfx::ImageSkia();
#endif
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (icon_str == "googlePay" || icon_str == "googlePayDark") {
    return gfx::ImageSkia();
  }
#endif
  // For other suggestion entries, get icon from PNG files.
  int icon_id = GetIconResourceID(icon_str);
  DCHECK_NE(kResourceNotFoundId, icon_id);
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id);
}
#endif  // !defined(OS_ANDROID)

}  // namespace autofill
