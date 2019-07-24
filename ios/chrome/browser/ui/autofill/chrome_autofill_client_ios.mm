// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/autofill_credit_card_filling_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/ios/browser/autofill_util.h"
#include "components/infobars/core/infobar.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/address_normalizer_factory.h"
#include "ios/chrome/browser/autofill/autocomplete_history_manager_factory.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/autofill/strike_database_factory.h"
#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/chrome/browser/infobars/infobar_utils.h"
#include "ios/chrome/browser/metrics/ukm_url_recorder.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/ssl/ios_security_state_tab_helper.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#include "ios/chrome/browser/ui/autofill/save_card_infobar_controller.h"
#include "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Creates and returns an infobar for saving credit cards.
std::unique_ptr<infobars::InfoBar> CreateSaveCardInfoBarMobile(
    std::unique_ptr<autofill::AutofillSaveCardInfoBarDelegateMobile> delegate) {
  SaveCardInfoBarController* controller = [[SaveCardInfoBarController alloc]
      initWithInfoBarDelegate:delegate.get()];
  return std::make_unique<InfoBarIOS>(controller, std::move(delegate));
}

}  // namespace

namespace autofill {

ChromeAutofillClientIOS::ChromeAutofillClientIOS(
    ios::ChromeBrowserState* browser_state,
    web::WebState* web_state,
    infobars::InfoBarManager* infobar_manager,
    id<AutofillClientIOSBridge> bridge,
    password_manager::PasswordManager* password_manager)
    : pref_service_(browser_state->GetPrefs()),
      sync_service_(
          ProfileSyncServiceFactory::GetForBrowserState(browser_state)),
      personal_data_manager_(PersonalDataManagerFactory::GetForBrowserState(
          browser_state->GetOriginalChromeBrowserState())),
      autocomplete_history_manager_(
          AutocompleteHistoryManagerFactory::GetForBrowserState(browser_state)),
      browser_state_(browser_state),
      web_state_(web_state),
      bridge_(bridge),
      identity_manager_(IdentityManagerFactory::GetForBrowserState(
          browser_state->GetOriginalChromeBrowserState())),
      payments_client_(std::make_unique<payments::PaymentsClient>(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              web_state_->GetBrowserState()->GetURLLoaderFactory()),
          identity_manager_,
          personal_data_manager_,
          web_state_->GetBrowserState()->IsOffTheRecord())),
      form_data_importer_(std::make_unique<FormDataImporter>(
          this,
          payments_client_.get(),
          personal_data_manager_,
          GetApplicationContext()->GetApplicationLocale())),
      infobar_manager_(infobar_manager),
      password_manager_(password_manager),
      unmask_controller_(browser_state->GetPrefs(),
                         browser_state->IsOffTheRecord()) {}

ChromeAutofillClientIOS::~ChromeAutofillClientIOS() {
  HideAutofillPopup();
}

void ChromeAutofillClientIOS::SetBaseViewController(
    UIViewController* base_view_controller) {
  base_view_controller_ = base_view_controller;
}

version_info::Channel ChromeAutofillClientIOS::GetChannel() const {
  return ::GetChannel();
}

PersonalDataManager* ChromeAutofillClientIOS::GetPersonalDataManager() {
  return personal_data_manager_;
}

AutocompleteHistoryManager*
ChromeAutofillClientIOS::GetAutocompleteHistoryManager() {
  return autocomplete_history_manager_;
}

PrefService* ChromeAutofillClientIOS::GetPrefs() {
  return pref_service_;
}

syncer::SyncService* ChromeAutofillClientIOS::GetSyncService() {
  return sync_service_;
}

signin::IdentityManager* ChromeAutofillClientIOS::GetIdentityManager() {
  return identity_manager_;
}

FormDataImporter* ChromeAutofillClientIOS::GetFormDataImporter() {
  return form_data_importer_.get();
}

payments::PaymentsClient* ChromeAutofillClientIOS::GetPaymentsClient() {
  return payments_client_.get();
}

StrikeDatabase* ChromeAutofillClientIOS::GetStrikeDatabase() {
  return StrikeDatabaseFactory::GetForBrowserState(
      browser_state_->GetOriginalChromeBrowserState());
}

ukm::UkmRecorder* ChromeAutofillClientIOS::GetUkmRecorder() {
  return GetApplicationContext()->GetUkmRecorder();
}

ukm::SourceId ChromeAutofillClientIOS::GetUkmSourceId() {
  return ukm::GetSourceIdForWebStateDocument(web_state_);
}

AddressNormalizer* ChromeAutofillClientIOS::GetAddressNormalizer() {
  if (base::FeatureList::IsEnabled(features::kAutofillAddressNormalizer))
    return AddressNormalizerFactory::GetInstance();
  return nullptr;
}

security_state::SecurityLevel
ChromeAutofillClientIOS::GetSecurityLevelForUmaHistograms() {
  auto* ios_security_state_tab_helper =
      IOSSecurityStateTabHelper::FromWebState(web_state_);

  // If there is no helper, return SECURITY_LEVEL_COUNT which won't be logged.
  if (!ios_security_state_tab_helper)
    return security_state::SecurityLevel::SECURITY_LEVEL_COUNT;

  return ios_security_state_tab_helper->GetSecurityLevel();
}

std::string ChromeAutofillClientIOS::GetPageLanguage() const {
  // TODO(crbug.com/912597): iOS vs other platforms extracts language from
  // the top level frame vs whatever frame directly holds the form.
  auto* translate_client = ChromeIOSTranslateClient::FromWebState(web_state_);
  if (translate_client) {
    auto* translate_manager = translate_client->GetTranslateManager();
    if (translate_manager)
      return translate_manager->GetLanguageState().original_language();
  }
  return std::string();
}

void ChromeAutofillClientIOS::ShowAutofillSettings(
    bool show_credit_card_settings) {
  NOTREACHED();
}

void ChromeAutofillClientIOS::ShowUnmaskPrompt(
    const CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_.ShowPrompt(
      // autofill::CardUnmaskPromptViewBridge manages its own lifetime, so
      // do not use std::unique_ptr<> here.
      new autofill::CardUnmaskPromptViewBridge(&unmask_controller_,
                                               base_view_controller_),
      card, reason, delegate);
}

void ChromeAutofillClientIOS::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  unmask_controller_.OnVerificationResult(result);
}

void ChromeAutofillClientIOS::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {
  NOTIMPLEMENTED();
}

void ChromeAutofillClientIOS::ConfirmMigrateLocalCardToCloud(
    std::unique_ptr<base::DictionaryValue> legal_message,
    const std::string& user_email,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {
  NOTIMPLEMENTED();
}

void ChromeAutofillClientIOS::ShowLocalCardMigrationResults(
    const bool has_server_error,
    const base::string16& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrationDeleteCardCallback delete_local_card_callback) {
  NOTIMPLEMENTED();
}

void ChromeAutofillClientIOS::ConfirmSaveAutofillProfile(
    const AutofillProfile& profile,
    base::OnceClosure callback) {
  // Since there is no confirmation needed to save an Autofill Profile,
  // running |callback| will proceed with saving |profile|.
  std::move(callback).Run();
}

void ChromeAutofillClientIOS::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
  DCHECK(options.show_prompt);
  infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
      std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
          /*upload=*/false, options, card,
          std::make_unique<base::DictionaryValue>(),
          /*upload_save_card_callback=*/UploadSaveCardPromptCallback(),
          /*local_save_card_callback=*/std::move(callback), GetPrefs(),
          payments_client_->is_off_the_record())));
}

void ChromeAutofillClientIOS::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  DCHECK(options.show_prompt);
  auto save_card_info_bar_delegate_mobile =
      std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
          /*upload=*/true, options, card, std::move(legal_message),
          /*upload_save_card_callback=*/std::move(callback),
          /*local_save_card_callback=*/LocalSaveCardPromptCallback(),
          GetPrefs(), payments_client_->is_off_the_record());
  // Allow user to save card only if legal messages are successfully parsed.
  // Legal messages are provided only for the upload case, not for local save.
  if (save_card_info_bar_delegate_mobile->LegalMessagesParsedSuccessfully()) {
    infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
        std::move(save_card_info_bar_delegate_mobile)));
  }
}

void ChromeAutofillClientIOS::CreditCardUploadCompleted(bool card_saved) {
  NOTIMPLEMENTED();
}

void ChromeAutofillClientIOS::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    base::OnceClosure callback) {
  auto infobar_delegate =
      std::make_unique<AutofillCreditCardFillingInfoBarDelegateMobile>(
          card, std::move(callback));
  auto* raw_delegate = infobar_delegate.get();
  if (infobar_manager_->AddInfoBar(
          ::CreateConfirmInfoBar(std::move(infobar_delegate)))) {
    raw_delegate->set_was_shown();
  }
}

bool ChromeAutofillClientIOS::HasCreditCardScanFeature() {
  return false;
}

void ChromeAutofillClientIOS::ScanCreditCard(
    const CreditCardScanCallback& callback) {
  NOTREACHED();
}

void ChromeAutofillClientIOS::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<Suggestion>& suggestions,
    bool /*unused_autoselect_first_suggestion*/,
    PopupType popup_type,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  [bridge_ showAutofillPopup:suggestions popupDelegate:delegate];
}

void ChromeAutofillClientIOS::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  NOTREACHED();
}

void ChromeAutofillClientIOS::HideAutofillPopup() {
  [bridge_ hideAutofillPopup];
}

bool ChromeAutofillClientIOS::IsAutocompleteEnabled() {
  return prefs::IsAutocompleteEnabled(GetPrefs());
}

void ChromeAutofillClientIOS::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<FormStructure*>& forms) {
  password_manager_->ProcessAutofillPredictions(/*driver=*/nullptr, forms);
}

void ChromeAutofillClientIOS::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {}

bool ChromeAutofillClientIOS::IsContextSecure() {
  return IsContextSecureForWebState(web_state_);
}

bool ChromeAutofillClientIOS::ShouldShowSigninPromo() {
  return false;
}

bool ChromeAutofillClientIOS::AreServerCardsSupported() {
  return true;
}

void ChromeAutofillClientIOS::ExecuteCommand(int id) {
  NOTIMPLEMENTED();
}

void ChromeAutofillClientIOS::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run(ios::GetChromeBrowserProvider()->GetRiskData());
}

}  // namespace autofill
