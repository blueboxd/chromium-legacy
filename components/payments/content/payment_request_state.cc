// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_state.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_response_helper.h"
#include "components/payments/content/service_worker_payment_instrument.h"
#include "components/payments/core/autofill_card_validation.h"
#include "components/payments/core/autofill_payment_instrument.h"
#include "components/payments/core/features.h"
#include "components/payments/core/payment_instrument.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payments_experimental_features.h"
#include "content/public/common/content_features.h"

namespace payments {

PaymentRequestState::PaymentRequestState(
    content::WebContents* web_contents,
    const GURL& top_level_origin,
    const GURL& frame_origin,
    PaymentRequestSpec* spec,
    Delegate* delegate,
    const std::string& app_locale,
    autofill::PersonalDataManager* personal_data_manager,
    ContentPaymentRequestDelegate* payment_request_delegate,
    JourneyLogger* journey_logger)
    : is_ready_to_pay_(false),
      get_all_instruments_finished_(true),
      is_waiting_for_merchant_validation_(false),
      app_locale_(app_locale),
      spec_(spec),
      delegate_(delegate),
      personal_data_manager_(personal_data_manager),
      journey_logger_(journey_logger),
      are_requested_methods_supported_(
          !spec_->supported_card_networks().empty()),
      selected_shipping_profile_(nullptr),
      selected_shipping_option_error_profile_(nullptr),
      selected_contact_profile_(nullptr),
      invalid_shipping_profile_(nullptr),
      invalid_contact_profile_(nullptr),
      selected_instrument_(nullptr),
      number_of_pending_sw_payment_instruments_(0),
      payment_request_delegate_(payment_request_delegate),
      profile_comparator_(app_locale, *spec) {
  if (base::FeatureList::IsEnabled(::features::kServiceWorkerPaymentApps)) {
    DCHECK(web_contents);
    get_all_instruments_finished_ = false;
    bool may_crawl_for_installable_payment_apps =
        PaymentsExperimentalFeatures::IsEnabled(
            features::kAlwaysAllowJustInTimePaymentApp) ||
        !spec_->supports_basic_card();

    ServiceWorkerPaymentAppFactory::GetInstance()->GetAllPaymentApps(
        web_contents,
        payment_request_delegate_->GetPaymentManifestWebDataService(),
        spec_->method_data(), may_crawl_for_installable_payment_apps,
        base::BindOnce(&PaymentRequestState::GetAllPaymentAppsCallback,
                       weak_ptr_factory_.GetWeakPtr(), web_contents,
                       top_level_origin, frame_origin),
        base::BindOnce([]() {
          /* Nothing needs to be done after writing cache. This callback is used
           * only in tests. */
        }));
  } else {
    PopulateProfileCache();
    SetDefaultProfileSelections();
  }
  spec_->AddObserver(this);
}

PaymentRequestState::~PaymentRequestState() {}

void PaymentRequestState::GetAllPaymentAppsCallback(
    content::WebContents* web_contents,
    const GURL& top_level_origin,
    const GURL& frame_origin,
    content::PaymentAppProvider::PaymentApps apps,
    ServiceWorkerPaymentAppFactory::InstallablePaymentApps installable_apps,
    const std::string& error_message) {
  number_of_pending_sw_payment_instruments_ =
      apps.size() + installable_apps.size();
  get_all_payment_apps_error_ = error_message;
  if (number_of_pending_sw_payment_instruments_ == 0U) {
    FinishedGetAllSWPaymentInstruments();
    return;
  }

  for (auto& app : apps) {
    std::unique_ptr<ServiceWorkerPaymentInstrument> instrument =
        std::make_unique<ServiceWorkerPaymentInstrument>(
            web_contents->GetBrowserContext(), top_level_origin, frame_origin,
            spec_, std::move(app.second), payment_request_delegate_);
    instrument->ValidateCanMakePayment(
        base::BindOnce(&PaymentRequestState::OnSWPaymentInstrumentValidated,
                       weak_ptr_factory_.GetWeakPtr()));
    available_instruments_.push_back(std::move(instrument));
  }

  for (auto& installable_app : installable_apps) {
    std::unique_ptr<ServiceWorkerPaymentInstrument> instrument =
        std::make_unique<ServiceWorkerPaymentInstrument>(
            web_contents, top_level_origin, frame_origin, spec_,
            std::move(installable_app.second), installable_app.first.spec(),
            payment_request_delegate_);
    instrument->ValidateCanMakePayment(
        base::BindOnce(&PaymentRequestState::OnSWPaymentInstrumentValidated,
                       weak_ptr_factory_.GetWeakPtr()));
    available_instruments_.push_back(std::move(instrument));
  }
}

void PaymentRequestState::OnSWPaymentInstrumentValidated(
    ServiceWorkerPaymentInstrument* instrument,
    bool result) {
  // Remove service worker payment instruments failed on validation.
  if (!result) {
    for (size_t i = 0; i < available_instruments_.size(); i++) {
      if (available_instruments_[i].get() == instrument) {
        available_instruments_.erase(available_instruments_.begin() + i);
        break;
      }
    }
  }

  if (--number_of_pending_sw_payment_instruments_ > 0)
    return;

  FinishedGetAllSWPaymentInstruments();
}

void PaymentRequestState::FinishedGetAllSWPaymentInstruments() {
  PopulateProfileCache();
  SetDefaultProfileSelections();

  get_all_instruments_finished_ = true;
  are_requested_methods_supported_ |= !available_instruments_.empty();
  NotifyOnGetAllPaymentInstrumentsFinished();
  NotifyInitialized();

  // Fulfill the pending CanMakePayment call.
  if (can_make_payment_callback_) {
    CheckCanMakePayment(can_make_payment_legacy_mode_,
                        std::move(can_make_payment_callback_));
  }

  // Fulfill the pending HasEnrolledInstrument call.
  if (has_enrolled_instrument_callback_)
    CheckHasEnrolledInstrument(std::move(has_enrolled_instrument_callback_));

  // Fulfill the pending AreRequestedMethodsSupported call.
  if (are_requested_methods_supported_callback_)
    CheckRequestedMethodsSupported(
        std::move(are_requested_methods_supported_callback_));
}

void PaymentRequestState::OnPaymentResponseReady(
    mojom::PaymentResponsePtr payment_response) {
  delegate_->OnPaymentResponseAvailable(std::move(payment_response));
}

void PaymentRequestState::OnPaymentResponseError(
    const std::string& error_message) {
  delegate_->OnPaymentResponseError(error_message);
}

void PaymentRequestState::OnSpecUpdated() {
  autofill::AutofillProfile* selected_shipping_profile =
      selected_shipping_profile_;
  autofill::AutofillProfile* selected_contact_profile =
      selected_contact_profile_;

  if (spec_->current_update_reason() ==
      PaymentRequestSpec::UpdateReason::RETRY) {
    if (spec_->has_shipping_address_error() && selected_shipping_profile) {
      invalid_shipping_profile_ = selected_shipping_profile;
      selected_shipping_profile_ = nullptr;
    }

    if (spec_->has_payer_error() && selected_contact_profile) {
      invalid_contact_profile_ = selected_contact_profile;
      selected_contact_profile_ = nullptr;
    }
  }

  if (spec_->selected_shipping_option_error().empty()) {
    selected_shipping_option_error_profile_ = nullptr;
  } else {
    selected_shipping_option_error_profile_ = selected_shipping_profile;
    selected_shipping_profile_ = nullptr;
    if (spec_->has_shipping_address_error() && selected_shipping_profile) {
      invalid_shipping_profile_ = selected_shipping_profile;
    }
  }

  is_waiting_for_merchant_validation_ = false;
  UpdateIsReadyToPayAndNotifyObservers();
}

void PaymentRequestState::CanMakePayment(bool legacy_mode,
                                         StatusCallback callback) {
  if (!get_all_instruments_finished_) {
    DCHECK(!can_make_payment_callback_);
    can_make_payment_callback_ = std::move(callback);
    can_make_payment_legacy_mode_ = legacy_mode;
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PaymentRequestState::CheckCanMakePayment,
                                weak_ptr_factory_.GetWeakPtr(), legacy_mode,
                                std::move(callback)));
}

void PaymentRequestState::CheckCanMakePayment(bool legacy_mode,
                                              StatusCallback callback) {
  DCHECK(get_all_instruments_finished_);
  if (!legacy_mode) {
    std::move(callback).Run(are_requested_methods_supported_);
    return;
  }

  // Legacy mode: fall back to also checking if an instrument is enrolled.
  bool can_make_payment_value = false;
  for (const auto& instrument : available_instruments_) {
    if (instrument->IsValidForCanMakePayment()) {
      can_make_payment_value = true;
      break;
    }
  }
  std::move(callback).Run(can_make_payment_value);
}

void PaymentRequestState::HasEnrolledInstrument(StatusCallback callback) {
  if (!get_all_instruments_finished_) {
    DCHECK(!has_enrolled_instrument_callback_);
    has_enrolled_instrument_callback_ = std::move(callback);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PaymentRequestState::CheckHasEnrolledInstrument,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentRequestState::CheckHasEnrolledInstrument(StatusCallback callback) {
  DCHECK(get_all_instruments_finished_);
  bool has_enrolled_instrument_value = false;
  for (const auto& instrument : available_instruments_) {
    if (instrument->IsValidForCanMakePayment()) {
      has_enrolled_instrument_value = true;
      break;
    }
  }
  std::move(callback).Run(has_enrolled_instrument_value);
}

void PaymentRequestState::AreRequestedMethodsSupported(
    MethodsSupportedCallback callback) {
  if (!get_all_instruments_finished_) {
    are_requested_methods_supported_callback_ = std::move(callback);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PaymentRequestState::CheckRequestedMethodsSupported,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentRequestState::CheckRequestedMethodsSupported(
    MethodsSupportedCallback callback) {
  DCHECK(get_all_instruments_finished_);

  std::move(callback).Run(are_requested_methods_supported_,
                          get_all_payment_apps_error_);
}

std::string PaymentRequestState::GetAuthenticatedEmail() const {
  return payment_request_delegate_->GetAuthenticatedEmail();
}

void PaymentRequestState::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void PaymentRequestState::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PaymentRequestState::GeneratePaymentResponse() {
  DCHECK(is_ready_to_pay());

  // Once the response is ready, will call back into OnPaymentResponseReady.
  response_helper_ = std::make_unique<PaymentResponseHelper>(
      app_locale_, spec_, selected_instrument_, payment_request_delegate_,
      selected_shipping_profile_, selected_contact_profile_, this);
}

void PaymentRequestState::OnPaymentAppWindowClosed() {
  DCHECK(selected_instrument_);
  response_helper_.reset();
  selected_instrument_->OnPaymentAppWindowClosed();
}

void PaymentRequestState::RecordUseStats() {
  if (spec_->request_shipping()) {
    DCHECK(selected_shipping_profile_);
    personal_data_manager_->RecordUseOf(*selected_shipping_profile_);
  }

  if (spec_->request_payer_name() || spec_->request_payer_email() ||
      spec_->request_payer_phone()) {
    DCHECK(selected_contact_profile_);

    // If the same address was used for both contact and shipping, the stats
    // should only be updated once.
    if (!spec_->request_shipping() || (selected_shipping_profile_->guid() !=
                                       selected_contact_profile_->guid())) {
      personal_data_manager_->RecordUseOf(*selected_contact_profile_);
    }
  }

  selected_instrument_->RecordUse();
}

void PaymentRequestState::AddAutofillPaymentInstrument(
    bool selected,
    const autofill::CreditCard& card) {
  std::string basic_card_network =
      autofill::data_util::GetPaymentRequestData(card.network())
          .basic_card_issuer_network;
  if (!spec_->supported_card_networks_set().count(basic_card_network) ||
      !spec_->supported_card_types_set().count(card.card_type())) {
    return;
  }

  // The total number of card types: credit, debit, prepaid, unknown.
  constexpr size_t kTotalNumberOfCardTypes = 4U;

  // Whether the card type (credit, debit, prepaid) matches thetype that the
  // merchant has requested exactly. This should be false for unknown card
  // types, if the merchant cannot accept some card types.
  bool matches_merchant_card_type_exactly =
      card.card_type() != autofill::CreditCard::CARD_TYPE_UNKNOWN ||
      spec_->supported_card_types_set().size() == kTotalNumberOfCardTypes;

  // AutofillPaymentInstrument makes a copy of |card| so it is effectively
  // owned by this object.
  auto instrument = std::make_unique<AutofillPaymentInstrument>(
      basic_card_network, card, matches_merchant_card_type_exactly,
      shipping_profiles_, app_locale_, payment_request_delegate_);
  available_instruments_.push_back(std::move(instrument));

  if (selected) {
    SetSelectedInstrument(available_instruments_.back().get(),
                          SectionSelectionStatus::kAddedSelected);
  }
}

void PaymentRequestState::AddAutofillShippingProfile(
    bool selected,
    const autofill::AutofillProfile& profile) {
  profile_cache_.push_back(
      std::make_unique<autofill::AutofillProfile>(profile));
  // TODO(tmartino): Implement deduplication rules specific to shipping
  // profiles.
  autofill::AutofillProfile* new_cached_profile = profile_cache_.back().get();
  shipping_profiles_.push_back(new_cached_profile);

  if (selected) {
    SetSelectedShippingProfile(new_cached_profile,
                               SectionSelectionStatus::kAddedSelected);
  }
}

void PaymentRequestState::AddAutofillContactProfile(
    bool selected,
    const autofill::AutofillProfile& profile) {
  profile_cache_.push_back(
      std::make_unique<autofill::AutofillProfile>(profile));
  autofill::AutofillProfile* new_cached_profile = profile_cache_.back().get();
  contact_profiles_.push_back(new_cached_profile);

  if (selected) {
    SetSelectedContactProfile(new_cached_profile,
                              SectionSelectionStatus::kAddedSelected);
  }
}

void PaymentRequestState::SetSelectedShippingOption(
    const std::string& shipping_option_id) {
  spec_->StartWaitingForUpdateWith(
      PaymentRequestSpec::UpdateReason::SHIPPING_OPTION);
  // This will inform the merchant and will lead to them calling updateWith with
  // new PaymentDetails.
  delegate_->OnShippingOptionIdSelected(shipping_option_id);
}

void PaymentRequestState::SetSelectedShippingProfile(
    autofill::AutofillProfile* profile,
    SectionSelectionStatus selection_status) {
  spec_->StartWaitingForUpdateWith(
      PaymentRequestSpec::UpdateReason::SHIPPING_ADDRESS);
  selected_shipping_profile_ = profile;

  // Changing the shipping address clears shipping address validation errors
  // from retry().
  invalid_shipping_profile_ = nullptr;

  // The user should not be able to click on pay until the callback from the
  // merchant.
  is_waiting_for_merchant_validation_ = true;

  // Start the normalization of the shipping address.
  payment_request_delegate_->GetAddressNormalizer()->NormalizeAddressAsync(
      *selected_shipping_profile_, /*timeout_seconds=*/2,
      base::BindOnce(&PaymentRequestState::OnAddressNormalized,
                     weak_ptr_factory_.GetWeakPtr()));
  IncrementSelectionStatus(JourneyLogger::Section::SECTION_SHIPPING_ADDRESS,
                           selection_status);
}

void PaymentRequestState::SetSelectedContactProfile(
    autofill::AutofillProfile* profile,
    SectionSelectionStatus selection_status) {
  selected_contact_profile_ = profile;

  // Changing the contact information clears contact information validation
  // errors from retry().
  invalid_contact_profile_ = nullptr;

  UpdateIsReadyToPayAndNotifyObservers();

  if (IsPaymentAppInvoked()) {
    delegate_->OnPayerInfoSelected(
        response_helper_->GeneratePayerDetail(profile));
  }
  IncrementSelectionStatus(JourneyLogger::Section::SECTION_CONTACT_INFO,
                           selection_status);
}

void PaymentRequestState::SetSelectedInstrument(
    PaymentInstrument* instrument,
    SectionSelectionStatus selection_status) {
  selected_instrument_ = instrument;
  UpdateIsReadyToPayAndNotifyObservers();
  IncrementSelectionStatus(JourneyLogger::Section::SECTION_PAYMENT_METHOD,
                           selection_status);
}

void PaymentRequestState::IncrementSelectionStatus(
    JourneyLogger::Section section,
    SectionSelectionStatus selection_status) {
  switch (selection_status) {
    case SectionSelectionStatus::kSelected:
      journey_logger_->IncrementSelectionChanges(section);
      break;
    case SectionSelectionStatus::kEditedSelected:
      journey_logger_->IncrementSelectionEdits(section);
      break;
    case SectionSelectionStatus::kAddedSelected:
      journey_logger_->IncrementSelectionAdds(section);
      break;
    default:
      NOTREACHED();
  }
}

const std::string& PaymentRequestState::GetApplicationLocale() {
  return app_locale_;
}

autofill::PersonalDataManager* PaymentRequestState::GetPersonalDataManager() {
  return personal_data_manager_;
}

autofill::RegionDataLoader* PaymentRequestState::GetRegionDataLoader() {
  return payment_request_delegate_->GetRegionDataLoader();
}

bool PaymentRequestState::IsPaymentAppInvoked() const {
  return !!response_helper_;
}

autofill::AddressNormalizer* PaymentRequestState::GetAddressNormalizer() {
  return payment_request_delegate_->GetAddressNormalizer();
}

bool PaymentRequestState::IsInitialized() const {
  return get_all_instruments_finished_;
}

void PaymentRequestState::SelectDefaultShippingAddressAndNotifyObservers() {
  // Only pre-select an address if the merchant provided at least one selected
  // shipping option, and the top profile is complete. Assumes that profiles
  // have already been sorted for completeness and frecency.
  if (!shipping_profiles().empty() && spec_->selected_shipping_option() &&
      profile_comparator()->IsShippingComplete(shipping_profiles_[0])) {
    selected_shipping_profile_ = shipping_profiles()[0];
  }
  // Record the missing required fields (if any) of the most complete shipping
  // profile.
  profile_comparator()->RecordMissingFieldsOfShippingProfile(
      shipping_profiles().empty() ? nullptr : shipping_profiles()[0]);
  UpdateIsReadyToPayAndNotifyObservers();
}

void PaymentRequestState::PopulateProfileCache() {
  std::vector<autofill::AutofillProfile*> profiles =
      personal_data_manager_->GetProfilesToSuggest();

  std::vector<autofill::AutofillProfile*> raw_profiles_for_filtering;
  raw_profiles_for_filtering.reserve(profiles.size());

  // PaymentRequest may outlive the Profiles returned by the Data Manager.
  // Thus, we store copies, and return a vector of pointers to these copies
  // whenever Profiles are requested.
  for (size_t i = 0; i < profiles.size(); i++) {
    profile_cache_.push_back(
        std::make_unique<autofill::AutofillProfile>(*profiles[i]));
    raw_profiles_for_filtering.push_back(profile_cache_.back().get());
  }

  contact_profiles_ = profile_comparator()->FilterProfilesForContact(
      raw_profiles_for_filtering);
  shipping_profiles_ = profile_comparator()->FilterProfilesForShipping(
      raw_profiles_for_filtering);

  // Set the number of suggestions shown for the sections requested by the
  // merchant.
  if (spec_->request_payer_name() || spec_->request_payer_phone() ||
      spec_->request_payer_email()) {
    bool has_complete_contact =
        contact_profiles_.empty()
            ? false
            : profile_comparator()->IsContactInfoComplete(contact_profiles_[0]);
    journey_logger_->SetNumberOfSuggestionsShown(
        JourneyLogger::Section::SECTION_CONTACT_INFO, contact_profiles_.size(),
        has_complete_contact);
  }
  if (spec_->request_shipping()) {
    bool has_complete_shipping =
        shipping_profiles_.empty()
            ? false
            : profile_comparator()->IsShippingComplete(shipping_profiles_[0]);
    journey_logger_->SetNumberOfSuggestionsShown(
        JourneyLogger::Section::SECTION_SHIPPING_ADDRESS,
        shipping_profiles_.size(), has_complete_shipping);
  }

  // Create the list of available instruments. A copy of each card will be made
  // by their respective AutofillPaymentInstrument.
  const std::vector<autofill::CreditCard*>& cards =
      personal_data_manager_->GetCreditCardsToSuggest(
          /*include_server_cards=*/base::FeatureList::IsEnabled(
              payments::features::kReturnGooglePayInBasicCard));
  for (autofill::CreditCard* card : cards)
    AddAutofillPaymentInstrument(/*selected=*/false, *card);
}

void PaymentRequestState::SetDefaultProfileSelections() {
  // Contact profiles were ordered by completeness in addition to frecency;
  // the first one is the best default selection.
  if (!contact_profiles().empty() &&
      profile_comparator()->IsContactInfoComplete(contact_profiles_[0]))
    selected_contact_profile_ = contact_profiles()[0];

  // Record the missing required fields (if any) of the most complete contact
  // profile.
  profile_comparator()->RecordMissingFieldsOfContactProfile(
      contact_profiles().empty() ? nullptr : contact_profiles()[0]);

  // Sort instruments.
  PaymentInstrument::SortInstruments(&available_instruments_);

  selected_instrument_ = nullptr;
  if (!available_instruments_.empty() &&
      available_instruments_[0]->IsCompleteForPayment() &&
      available_instruments_[0]->IsExactlyMatchingMerchantRequest()) {
    selected_instrument_ = available_instruments_[0].get();
  }

  // Record the missing required payment fields when no complete payment
  // info exists.
  if (available_instruments_.empty()) {
    if (spec_->supports_basic_card()) {
      // All fields are missing when basic-card is requested but no card exits.
      base::UmaHistogramSparse("PaymentRequest.MissingPaymentFields",
                               CREDIT_CARD_EXPIRED | CREDIT_CARD_NO_CARDHOLDER |
                                   CREDIT_CARD_NO_NUMBER |
                                   CREDIT_CARD_NO_BILLING_ADDRESS);
    }
  } else if (available_instruments_[0]->type() ==
             PaymentInstrument::Type::AUTOFILL) {
    // Record the missing fields (if any) of the most complete instrument when
    // it's autofill based. SW based instruments are always complete.
    static_cast<const AutofillPaymentInstrument*>(
        available_instruments_[0].get())
        ->RecordMissingFieldsForInstrument();
  }

  SelectDefaultShippingAddressAndNotifyObservers();

  journey_logger_->SetNumberOfSuggestionsShown(
      JourneyLogger::Section::SECTION_PAYMENT_METHOD,
      available_instruments().size(), selected_instrument_);
}

void PaymentRequestState::UpdateIsReadyToPayAndNotifyObservers() {
  is_ready_to_pay_ =
      ArePaymentDetailsSatisfied() && ArePaymentOptionsSatisfied();
  NotifyOnSelectedInformationChanged();
}

void PaymentRequestState::NotifyOnGetAllPaymentInstrumentsFinished() {
  for (auto& observer : observers_)
    observer.OnGetAllPaymentInstrumentsFinished();
}

void PaymentRequestState::NotifyOnSelectedInformationChanged() {
  for (auto& observer : observers_)
    observer.OnSelectedInformationChanged();
}

bool PaymentRequestState::ArePaymentDetailsSatisfied() {
  // There is no need to check for supported networks, because only supported
  // instruments are listed/created in the flow.
  return selected_instrument_ != nullptr &&
         selected_instrument_->IsCompleteForPayment();
}

bool PaymentRequestState::ArePaymentOptionsSatisfied() {
  if (is_waiting_for_merchant_validation_)
    return false;

  if (!profile_comparator()->IsShippingComplete(selected_shipping_profile_))
    return false;

  if (spec_->request_shipping() && !spec_->selected_shipping_option())
    return false;

  return profile_comparator()->IsContactInfoComplete(selected_contact_profile_);
}

void PaymentRequestState::OnAddressNormalized(
    bool success,
    const autofill::AutofillProfile& normalized_profile) {
  delegate_->OnShippingAddressSelected(
      data_util::GetPaymentAddressFromAutofillProfile(normalized_profile,
                                                      app_locale_));
}

}  // namespace payments
