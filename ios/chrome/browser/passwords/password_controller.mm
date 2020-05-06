// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_controller.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/renderer_id.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/js_password_manager.h"
#import "components/password_manager/ios/password_suggestion_helper.h"
#include "components/password_manager/ios/unique_id_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/passwords/credential_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/ios_chrome_update_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/ios_password_infobar_controller.h"
#import "ios/chrome/browser/passwords/notify_auto_signin_view_controller.h"
#import "ios/chrome/browser/passwords/password_form_filler.h"
#include "ios/chrome/browser/passwords/password_manager_features.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/password_breach_commands.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_password_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/common/url_scheme_util.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#include "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormActivityObserverBridge;
using autofill::FormData;
using autofill::PasswordFormGenerationData;
using autofill::PasswordForm;
using autofill::FormRendererId;
using autofill::FieldRendererId;
using base::SysNSStringToUTF16;
using base::SysUTF16ToNSString;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;
using password_manager::metrics_util::LogPasswordDropdownShown;
using password_manager::metrics_util::PasswordDropdownState;
using password_manager::AccountSelectFillData;
using password_manager::FillData;
using password_manager::GetPageURLAndCheckTrustLevel;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordGenerationFrameHelper;
using password_manager::PasswordManager;
using password_manager::PasswordManagerClient;
using password_manager::PasswordManagerDriver;
using password_manager::SerializePasswordFormFillData;

namespace {
// Types of password infobars to display.
enum class PasswordInfoBarType { SAVE, UPDATE };

// Password is considered not generated when user edits it below 4 characters.
constexpr int kMinimumLengthForEditedPassword = 4;

// Duration for notify user auto-sign in dialog being displayed.
constexpr int kNotifyAutoSigninDuration = 3;  // seconds

// The string ' •••' appended to the username in the suggestion.
NSString* const kSuggestionSuffix = @" ••••••••";
}  // namespace

@interface PasswordController ()<PasswordSuggestionHelperDelegate>

// View controller for auto sign-in notification, owned by this
// PasswordController.
@property(nonatomic, strong)
    NotifyUserAutoSigninViewController* notifyAutoSigninViewController;

// Helper contains common password form processing logic.
@property(nonatomic, readonly) PasswordFormHelper* formHelper;

// Helper contains common password suggestion logic.
@property(nonatomic, readonly) PasswordSuggestionHelper* suggestionHelper;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// Tracks if current password is generated.
@property(nonatomic, assign) BOOL isPasswordGenerated;

// Tracks field when current password was generated.
@property(nonatomic, copy) NSString* passwordGeneratedIdentifier;

// Tracks current potential generated password until accepted or rejected.
@property(nonatomic, copy) NSString* generatedPotentialPassword;

@end

@interface PasswordController () <FormSuggestionProvider,
                                  PasswordFormFiller,
                                  FormActivityObserver>

// Informs the |_passwordManager| of the password forms (if any were present)
// that have been found on the page.
- (void)didFinishPasswordFormExtraction:(const std::vector<FormData>&)forms
                        withMaxUniqueID:(uint32_t)maxID;

// Finds all password forms in DOM and sends them to the password store for
// fetching stored credentials.
- (void)findPasswordFormsAndSendThemToPasswordStore;

// Displays infobar for |form| with |type|. If |type| is UPDATE, the user
// is prompted to update the password. If |type| is SAVE, the user is prompted
// to save the password.
- (void)showInfoBarForForm:(std::unique_ptr<PasswordFormManagerForUI>)form
               infoBarType:(PasswordInfoBarType)type
                    manual:(BOOL)manual;

// Removes infobar for given |type| if it exists. If it is not found the
// request is silently ignored (because that use case is expected).
- (void)removeInfoBarOfType:(PasswordInfoBarType)type manual:(BOOL)manual;

// Hides auto sign-in notification. Removes the view from superview and destroys
// the controller.
// TODO(crbug.com/435048): Animate disappearance.
- (void)hideAutosigninNotification;

@end

@implementation PasswordController {
  std::unique_ptr<PasswordManager> _passwordManager;
  std::unique_ptr<PasswordGenerationFrameHelper> _passwordGenerationHelper;
  std::unique_ptr<PasswordManagerClient> _passwordManagerClient;
  std::unique_ptr<PasswordManagerDriver> _passwordManagerDriver;
  std::unique_ptr<CredentialManager> _credentialManager;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe form activity in |_webState|.
  std::unique_ptr<FormActivityObserverBridge> _formActivityObserverBridge;

  // Timer for hiding "Signing in as ..." notification.
  base::OneShotTimer _notifyAutoSigninTimer;

  // User credential waiting to be displayed in autosign-in snackbar, once tab
  // becomes active.
  std::unique_ptr<autofill::PasswordForm> _pendingAutoSigninPasswordForm;

  // Form data for password generation on this page.
  std::map<base::string16, PasswordFormGenerationData> _formGenerationData;

  NSString* _lastTypedfieldIdentifier;
  NSString* _lastTypedValue;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [self initWithWebState:webState client:nullptr];
  return self;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                          client:(std::unique_ptr<PasswordManagerClient>)
                                     passwordManagerClient {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _formActivityObserverBridge =
        std::make_unique<FormActivityObserverBridge>(_webState, self);
    _formHelper =
        [[PasswordFormHelper alloc] initWithWebState:webState delegate:self];
    _suggestionHelper =
        [[PasswordSuggestionHelper alloc] initWithDelegate:self];
    if (passwordManagerClient)
      _passwordManagerClient = std::move(passwordManagerClient);
    else
      _passwordManagerClient.reset(new IOSChromePasswordManagerClient(self));
    _passwordManager.reset(new PasswordManager(_passwordManagerClient.get()));
    _passwordManagerDriver.reset(new IOSChromePasswordManagerDriver(self));

    if (!_passwordManagerClient->IsIncognito()) {
      _passwordGenerationHelper.reset(new PasswordGenerationFrameHelper(
          _passwordManagerClient.get(), _passwordManagerDriver.get()));
    }

    if (base::FeatureList::IsEnabled(features::kCredentialManager)) {
      _credentialManager = std::make_unique<CredentialManager>(
          _passwordManagerClient.get(), _webState);
    }
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

#pragma mark - Properties

- (id<PasswordFormFiller>)passwordFormFiller {
  return self;
}

- (ukm::SourceId)ukmSourceId {
  return _webState ? ukm::GetSourceIdForWebStateDocument(_webState)
                   : ukm::kInvalidSourceId;
}

- (PasswordManagerClient*)passwordManagerClient {
  return _passwordManagerClient.get();
}

- (PasswordManagerDriver*)passwordManagerDriver {
  return _passwordManagerDriver.get();
}

#pragma mark - PasswordFormFiller

- (void)findAndFillPasswordForms:(NSString*)username
                        password:(NSString*)password
               completionHandler:(void (^)(BOOL))completionHandler {
  [self.formHelper findAndFillPasswordFormsWithUserName:username
                                               password:password
                                      completionHandler:completionHandler];
}

#pragma mark - CRWWebStateObserver

// If Tab was shown, and there is a pending PasswordForm, display autosign-in
// notification.
- (void)webStateWasShown:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_pendingAutoSigninPasswordForm) {
    [self showAutosigninNotification:std::move(_pendingAutoSigninPasswordForm)];
    _pendingAutoSigninPasswordForm.reset();
  }
}

// If Tab was hidden, hide auto sign-in notification.
- (void)webStateWasHidden:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self hideAutosigninNotification];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  if (!navigation->HasCommitted() || navigation->IsSameDocument())
    return;

  if (!GetPageURLAndCheckTrustLevel(webState, nullptr))
    return;

  // On non-iOS platforms navigations initiated by link click are excluded from
  // navigations which might be form submssions. On iOS there is no easy way to
  // check that the navigation is link initiated, so it is skipped. It should
  // not be so important since it is unlikely that the user clicks on a link
  // after filling password form w/o submitting it.
  self.passwordManager->DidNavigateMainFrame(
      /*form_may_be_submitted=*/navigation->IsRendererInitiated());
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  // Clear per-page state.
  [self.suggestionHelper resetForNewPage];

  // Retrieve the identity of the page. In case the page might be malicous,
  // returns early.
  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webState, &pageURL))
    return;

  if (!web::UrlHasWebScheme(pageURL))
    return;

  if (!webState->ContentIsHTML()) {
    // If the current page is not HTML, it does not contain any HTML forms.
    UniqueIDTabHelper* uniqueIDTabHelper =
        UniqueIDTabHelper::FromWebState(_webState);
    uint32_t maxUniqueID = uniqueIDTabHelper->GetNextAvailableRendererID();
    [self didFinishPasswordFormExtraction:std::vector<FormData>()
                          withMaxUniqueID:maxUniqueID];
  }

  [self findPasswordFormsAndSendThemToPasswordStore];
}

- (void)webState:(web::WebState*)webState
    frameDidBecomeAvailable:(web::WebFrame*)web_frame {
  DCHECK_EQ(_webState, webState);
  DCHECK(web_frame);
  UniqueIDTabHelper* uniqueIDTabHelper =
      UniqueIDTabHelper::FromWebState(_webState);
  uint32_t nextAvailableRendererID =
      uniqueIDTabHelper->GetNextAvailableRendererID();
  [self.formHelper setUpForUniqueIDsWithInitialState:nextAvailableRendererID];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _formActivityObserverBridge.reset();
    _webState = nullptr;
  }
  _passwordManagerDriver.reset();
  _passwordManager.reset();
  _passwordManagerClient.reset();
  _credentialManager.reset();
  _formGenerationData.clear();
  _isPasswordGenerated = NO;
  _lastTypedfieldIdentifier = nil;
  _lastTypedValue = nil;
}

#pragma mark - FormSuggestionProvider

- (id<FormSuggestionProvider>)suggestionProvider {
  return self;
}

- (void)checkIfSuggestionsAvailableForForm:(NSString*)formName
                              uniqueFormID:(FormRendererId)uniqueFormID
                           fieldIdentifier:(NSString*)fieldIdentifier
                             uniqueFieldID:(FieldRendererId)uniqueFieldID
                                 fieldType:(NSString*)fieldType
                                      type:(NSString*)type
                                typedValue:(NSString*)typedValue
                                   frameID:(NSString*)frameID
                               isMainFrame:(BOOL)isMainFrame
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  if (!GetPageURLAndCheckTrustLevel(webState, nullptr))
    return;
  [self.suggestionHelper
      checkIfSuggestionsAvailableForForm:formName
                            uniqueFormID:uniqueFormID
                         fieldIdentifier:fieldIdentifier
                           uniqueFieldID:uniqueFieldID
                               fieldType:fieldType
                                    type:type
                                 frameID:frameID
                             isMainFrame:isMainFrame
                                webState:webState
                       completionHandler:^(BOOL suggestionsAvailable) {
                         // Always display "Show All..." for password fields.
                         completion([fieldType isEqualToString:@"password"] ||
                                    suggestionsAvailable);
                       }];

  if (self.isPasswordGenerated &&
      [fieldIdentifier isEqualToString:self.passwordGeneratedIdentifier]) {
    // On other platforms, when the user clicks on generation field, we show
    // password in clear text. And the user has the possibility to edit it. On
    // iOS, it's harder to do (it's probably bad idea to change field type from
    // password to text). The decision was to give everything to the automatic
    // flow and avoid the manual flow, for a cleaner and simpler UI.
    if (typedValue.length < kMinimumLengthForEditedPassword) {
      self.isPasswordGenerated = NO;
      self.passwordGeneratedIdentifier = nil;
      self.passwordManager->OnPasswordNoLongerGenerated(
          self.passwordManagerDriver);
    } else {
      // Inject updated value to possibly update confirmation field.
      [self injectGeneratedPasswordForFormName:formName
                             generatedPassword:typedValue
                             completionHandler:nil];
    }
  }

  if (![fieldIdentifier isEqualToString:_lastTypedfieldIdentifier] ||
      ![typedValue isEqualToString:_lastTypedValue]) {
    // This method is called multiple times for the same user keystroke. Inform
    // only once the keystroke.
    _lastTypedfieldIdentifier = fieldIdentifier;
    _lastTypedValue = typedValue;

    self.passwordManager->UpdateStateOnUserInput(
        self.passwordManagerDriver, SysNSStringToUTF16(formName),
        SysNSStringToUTF16(fieldIdentifier), SysNSStringToUTF16(typedValue));
  }
}

- (void)retrieveSuggestionsForForm:(NSString*)formName
                      uniqueFormID:(FormRendererId)uniqueFormID
                   fieldIdentifier:(NSString*)fieldIdentifier
                     uniqueFieldID:(FieldRendererId)uniqueFieldID
                         fieldType:(NSString*)fieldType
                              type:(NSString*)type
                        typedValue:(NSString*)typedValue
                           frameID:(NSString*)frameID
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  if (!GetPageURLAndCheckTrustLevel(webState, nullptr))
    return;
  NSArray<FormSuggestion*>* rawSuggestions =
      [self.suggestionHelper retrieveSuggestionsWithFormID:uniqueFormID
                                           fieldIdentifier:uniqueFieldID
                                                 fieldType:fieldType];

  NSMutableArray<FormSuggestion*>* suggestions = [NSMutableArray array];
  bool isPasswordField = [fieldType isEqual:@"password"];
  for (FormSuggestion* rawSuggestion in rawSuggestions) {
    // 1) If this is a focus event or the field is empty show all suggestions.
    // Otherwise:
    // 2) If this is a username field then show only credentials with matching
    // prefixes. 3) If this is a password field then show suggestions only if
    // the field is empty.
    if (![type isEqual:@"focus"] && typedValue.length > 0 &&
        (isPasswordField || ![rawSuggestion.value hasPrefix:typedValue])) {
      continue;
    }
    [suggestions
        addObject:[FormSuggestion
                      suggestionWithValue:
                          [rawSuggestion.value
                              stringByAppendingString:kSuggestionSuffix]
                       displayDescription:rawSuggestion.displayDescription
                                     icon:nil
                               identifier:0]];
  }
  base::Optional<PasswordDropdownState> suggestion_state;
  if (suggestions.count) {
    suggestion_state = PasswordDropdownState::kStandard;
  }

  if ([self canGeneratePasswordForForm:formName
                       fieldIdentifier:fieldIdentifier
                             fieldType:fieldType]) {
    // Add "Suggest Password...".
    NSString* suggestPassword = GetNSString(IDS_IOS_SUGGEST_PASSWORD);
    [suggestions
        addObject:
            [FormSuggestion
                suggestionWithValue:suggestPassword
                 displayDescription:nil
                               icon:nil
                         identifier:autofill::
                                        POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY]];
    suggestion_state = PasswordDropdownState::kStandardGenerate;
  }

  if (suggestion_state) {
    LogPasswordDropdownShown(*suggestion_state,
                             _passwordManagerClient->IsIncognito());
  }

  completion([suggestions copy], self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                       form:(NSString*)formName
            fieldIdentifier:(NSString*)fieldIdentifier
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
  switch (suggestion.identifier) {
    case autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY: {
      // Navigate to the settings list.
      [self.delegate displaySavedPasswordList];
      completion();
      password_manager::metrics_util::LogPasswordDropdownItemSelected(
          password_manager::metrics_util::PasswordDropdownSelectedOption::
              kShowAll,
          _passwordManagerClient->IsIncognito());
      return;
    }
    case autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY: {
      // Don't call completion because current siggestion state should remain
      // whether user injects a generated password or cancels.
      [self generatePasswordForFormName:formName
                        fieldIdentifier:fieldIdentifier];
      password_manager::metrics_util::LogPasswordDropdownItemSelected(
          password_manager::metrics_util::PasswordDropdownSelectedOption::
              kGenerate,
          _passwordManagerClient->IsIncognito());
      return;
    }
    default: {
      password_manager::metrics_util::LogPasswordDropdownItemSelected(
          password_manager::metrics_util::PasswordDropdownSelectedOption::
              kPassword,
          _passwordManagerClient->IsIncognito());
      DCHECK([suggestion.value hasSuffix:kSuggestionSuffix]);
      NSString* username = [suggestion.value
          substringToIndex:suggestion.value.length - kSuggestionSuffix.length];
      std::unique_ptr<password_manager::FillData> fillData =
          [self.suggestionHelper getFillDataForUsername:username];

      if (!fillData) {
        completion();
        return;
      }

      [self.formHelper fillPasswordFormWithFillData:*fillData
                                  completionHandler:^(BOOL success) {
                                    completion();
                                  }];
      break;
    }
  }
}

#pragma mark - PasswordManagerClientDelegate

- (web::WebState*)webState {
  return _webState;
}

- (ChromeBrowserState*)browserState {
  return _webState ? ChromeBrowserState::FromBrowserState(
                         _webState->GetBrowserState())
                   : nullptr;
}

- (PasswordManager*)passwordManager {
  return _passwordManager.get();
}

- (const GURL&)lastCommittedURL {
  return self.formHelper.lastCommittedURL;
}

- (void)showSavePasswordInfoBar:
            (std::unique_ptr<PasswordFormManagerForUI>)formToSave
                         manual:(BOOL)manual {
  [self showInfoBarForForm:std::move(formToSave)
               infoBarType:PasswordInfoBarType::SAVE
                    manual:manual];
}

- (void)showUpdatePasswordInfoBar:
            (std::unique_ptr<PasswordFormManagerForUI>)formToUpdate
                           manual:(BOOL)manual {
  [self showInfoBarForForm:std::move(formToUpdate)
               infoBarType:PasswordInfoBarType::UPDATE
                    manual:manual];
}

- (void)removePasswordInfoBarManualFallback:(BOOL)manual {
  [self removeInfoBarOfType:PasswordInfoBarType::SAVE manual:manual];
  [self removeInfoBarOfType:PasswordInfoBarType::UPDATE manual:manual];
}

// Shows auto sign-in notification and schedules hiding it after 3 seconds.
// TODO(crbug.com/435048): Animate appearance.
- (void)showAutosigninNotification:
    (std::unique_ptr<autofill::PasswordForm>)formSignedIn {
  if (!_webState)
    return;

  // If a notification is already being displayed, hides the old one, then shows
  // the new one.
  if (self.notifyAutoSigninViewController) {
    _notifyAutoSigninTimer.Stop();
    [self hideAutosigninNotification];
  }

  // Creates view controller then shows the subview.
  self.notifyAutoSigninViewController =
      [[NotifyUserAutoSigninViewController alloc]
          initWithUsername:SysUTF16ToNSString(formSignedIn->username_value)
                   iconURL:formSignedIn->icon_url
          URLLoaderFactory:_webState->GetBrowserState()
                               ->GetSharedURLLoaderFactory()];
  TabIdTabHelper* tabIdHelper = TabIdTabHelper::FromWebState(_webState);
  if (![_delegate displaySignInNotification:self.notifyAutoSigninViewController
                                  fromTabId:tabIdHelper->tab_id()]) {
    // The notification was not shown. Store the password form in
    // |_pendingAutoSigninPasswordForm| to show the notification later.
    _pendingAutoSigninPasswordForm = std::move(formSignedIn);
    self.notifyAutoSigninViewController = nil;
    return;
  }

  // Hides notification after 3 seconds.
  __weak PasswordController* weakSelf = self;
  _notifyAutoSigninTimer.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kNotifyAutoSigninDuration),
      base::BindRepeating(^{
        [weakSelf hideAutosigninNotification];
      }));
}

- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType
                                  URL:(const GURL&)URL {
  [self.passwordBreachDispatcher showPasswordBreachForLeakType:leakType
                                                           URL:URL];
}

#pragma mark - PasswordManagerDriverDelegate

- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData
       completionHandler:(void (^)(BOOL))completionHandler {
  [self.suggestionHelper processWithPasswordFormFillData:formData];
  [self.formHelper fillPasswordForm:formData
                  completionHandler:completionHandler];
}

- (void)onNoSavedCredentials {
  [self.suggestionHelper processWithNoSavedCredentials];
}

- (PasswordGenerationFrameHelper*)passwordGenerationHelper {
  return _passwordGenerationHelper.get();
}

- (void)formEligibleForGenerationFound:(const PasswordFormGenerationData&)form {
  _formGenerationData[form.form_name] = form;
}

#pragma mark - PasswordFormHelperDelegate

- (void)formHelper:(PasswordFormHelper*)formHelper
     didSubmitForm:(const FormData&)form
       inMainFrame:(BOOL)inMainFrame {
  if (inMainFrame) {
    self.passwordManager->OnPasswordFormSubmitted(self.passwordManagerDriver,
                                                  form);
  } else {
    // Show a save prompt immediately because for iframes it is very hard to
    // figure out correctness of password forms submission.
    self.passwordManager->OnPasswordFormSubmittedNoChecksForiOS(
        self.passwordManagerDriver, form);
  }
}

#pragma mark - PasswordSuggestionHelperDelegate

- (void)suggestionHelperShouldTriggerFormExtraction:
    (PasswordSuggestionHelper*)suggestionHelper {
  [self findPasswordFormsAndSendThemToPasswordStore];
}

#pragma mark - Private methods

// The dispatcher used for ApplicationCommands.
- (id<ApplicationCommands>)applicationDispatcher {
  DCHECK(self.browser);
  DCHECK(self.browser->GetCommandDispatcher());
  return HandlerForProtocol(self.browser->GetCommandDispatcher(),
                            ApplicationCommands);
}

// The dispatcher used for PasswordBreachCommands.
- (id<PasswordBreachCommands>)passwordBreachDispatcher {
  DCHECK(self.browser);
  DCHECK(self.browser->GetCommandDispatcher());
  return HandlerForProtocol(self.browser->GetCommandDispatcher(),
                            PasswordBreachCommands);
}

- (void)didFinishPasswordFormExtraction:(const std::vector<FormData>&)forms
                        withMaxUniqueID:(uint32_t)maxID {
  // Do nothing if |self| has been detached.
  if (!self.passwordManager)
    return;

  if (!forms.empty()) {
    [self.suggestionHelper updateStateOnPasswordFormExtracted];
    UniqueIDTabHelper* uniqueIDTabHelper =
        UniqueIDTabHelper::FromWebState(_webState);
    // Update NextAvailableRendererId if a bigger value was extracted.
    if (uniqueIDTabHelper->GetNextAvailableRendererID() < maxID)
      uniqueIDTabHelper->SetNextAvailableRendererID(++maxID);

    // Invoke the password manager callback to autofill password forms
    // on the loaded page.
    self.passwordManager->OnPasswordFormsParsed(self.passwordManagerDriver,
                                                forms);
  } else {
    [self onNoSavedCredentials];
  }
  // Invoke the password manager callback to check if password was
  // accepted or rejected. If accepted, infobar is presented. If
  // rejected, the provisionally saved password is deleted. On Chrome
  // w/ a renderer, it is the renderer who calls OnPasswordFormsParsed()
  // and OnPasswordFormsRendered(). Bling has to improvised a bit on the
  // ordering of these two calls.
  self.passwordManager->OnPasswordFormsRendered(self.passwordManagerDriver,
                                                forms, true);
}

- (void)findPasswordFormsAndSendThemToPasswordStore {
  // Read all password forms from the page and send them to the password
  // manager.
  __weak PasswordController* weakSelf = self;
  [self.formHelper findPasswordFormsWithCompletionHandler:^(
                       const std::vector<FormData>& forms, uint32_t maxID) {
    [weakSelf didFinishPasswordFormExtraction:forms withMaxUniqueID:maxID];
  }];
}

- (InfoBarIOS*)findInfobarOfType:(InfobarType)infobarType manual:(BOOL)manual {
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(_webState);

  size_t count = infoBarManager->infobar_count();
  for (size_t i = 0; i < count; i++) {
    InfoBarIOS* infobar =
        static_cast<InfoBarIOS*>(infoBarManager->infobar_at(i));
    if (infobar->InfobarUIDelegate().infobarType == infobarType &&
        infobar->skip_banner() == manual)
      return infobar;
  }

  return nil;
}

- (void)removeInfoBarOfType:(PasswordInfoBarType)type manual:(BOOL)manual {
  if (!IsInfobarUIRebootEnabled())
    return;

  InfoBarIOS* infobar = nil;
  switch (type) {
    case PasswordInfoBarType::SAVE: {
      infobar = [self findInfobarOfType:InfobarType::kInfobarTypePasswordSave
                                 manual:manual];
      break;
    }
    case PasswordInfoBarType::UPDATE: {
      infobar = [self findInfobarOfType:InfobarType::kInfobarTypePasswordUpdate
                                 manual:manual];
      break;
    }
  }

  if (infobar) {
    InfoBarManagerImpl::FromWebState(_webState)->RemoveInfoBar(infobar);
  }
}

- (void)showInfoBarForForm:(std::unique_ptr<PasswordFormManagerForUI>)form
               infoBarType:(PasswordInfoBarType)type
                    manual:(BOOL)manual {
  if (!_webState)
    return;

  bool isSyncUser = false;
  if (self.browserState) {
    syncer::SyncService* sync_service =
        ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
    isSyncUser = password_bubble_experiment::IsSmartLockUser(sync_service);
  }
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(_webState);

  switch (type) {
    case PasswordInfoBarType::SAVE: {
      auto delegate = std::make_unique<IOSChromeSavePasswordInfoBarDelegate>(
          isSyncUser, /*password_update*/ false, std::move(form));
      delegate->set_dispatcher(self.applicationDispatcher);

      if (IsInfobarUIRebootEnabled()) {
        std::unique_ptr<InfoBarIOS> infobar;
        // If manual save, skip showing banner.
        bool skip_banner = manual;
        if (IsInfobarOverlayUIEnabled()) {
          infobar = std::make_unique<InfoBarIOS>(
              InfobarType::kInfobarTypePasswordSave, std::move(delegate),
              skip_banner);
        } else {
          InfobarPasswordCoordinator* coordinator = [[InfobarPasswordCoordinator
              alloc]
              initWithInfoBarDelegate:delegate.get()
                                 type:InfobarType::kInfobarTypePasswordSave];
          infobar = std::make_unique<InfoBarIOS>(
              coordinator, std::move(delegate), skip_banner);
        }
        infoBarManager->AddInfoBar(std::move(infobar),
                                   /*replace_existing=*/true);
      } else if (!manual) {
        IOSPasswordInfoBarController* controller =
            [[IOSPasswordInfoBarController alloc]
                initWithInfoBarDelegate:delegate.get()];
        infoBarManager->AddInfoBar(
            std::make_unique<InfoBarIOS>(controller, std::move(delegate)));
      }
      break;
    }
    case PasswordInfoBarType::UPDATE: {
      if (IsInfobarUIRebootEnabled()) {
        auto delegate = std::make_unique<IOSChromeSavePasswordInfoBarDelegate>(
            isSyncUser, /*password_update*/ true, std::move(form));
        delegate->set_dispatcher(self.applicationDispatcher);
        InfobarPasswordCoordinator* coordinator = [[InfobarPasswordCoordinator
            alloc]
            initWithInfoBarDelegate:delegate.get()
                               type:InfobarType::kInfobarTypePasswordUpdate];
        // If manual save, skip showing banner.
        std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
            coordinator, std::move(delegate), /*skip_banner=*/manual);
        infoBarManager->AddInfoBar(std::move(infobar),
                                   /*replace_existing=*/true);
      } else if (!manual) {
        IOSChromeUpdatePasswordInfoBarDelegate::Create(
            isSyncUser, infoBarManager, std::move(form),
            self.baseViewController, self.applicationDispatcher);
      }
      break;
    }
  }
}

- (void)hideAutosigninNotification {
  [self.notifyAutoSigninViewController willMoveToParentViewController:nil];
  [self.notifyAutoSigninViewController.view removeFromSuperview];
  [self.notifyAutoSigninViewController removeFromParentViewController];
  self.notifyAutoSigninViewController = nil;
}

- (BOOL)canGeneratePasswordForForm:(NSString*)formName
                   fieldIdentifier:(NSString*)fieldIdentifier
                         fieldType:(NSString*)fieldType {
  if (_passwordManagerClient->IsIncognito() ||
      !_passwordManagerDriver->GetPasswordGenerationHelper()
           ->IsGenerationEnabled(
               /*log_debug_data*/ true))
    return NO;
  if (![fieldType isEqualToString:@"password"])
    return NO;
  const PasswordFormGenerationData* generation_data =
      [self getFormForGenerationFromFormName:formName];
  if (!generation_data)
    return NO;

  NSString* newPasswordIdentifier =
      SysUTF16ToNSString(generation_data->new_password_element);
  if ([fieldIdentifier isEqualToString:newPasswordIdentifier])
    return YES;

  // Don't show password generation if the field is 'confirm password'.
  return NO;
}

- (const PasswordFormGenerationData*)getFormForGenerationFromFormName:
    (NSString*)formName {
  const base::string16 name = SysNSStringToUTF16(formName);
  if (_formGenerationData.find(name) != _formGenerationData.end()) {
    return &_formGenerationData[name];
  }
  return nullptr;
}

- (void)generatePasswordForFormName:(NSString*)formName
                    fieldIdentifier:(NSString*)fieldIdentifier {
  if (![self getFormForGenerationFromFormName:formName])
    return;

  // TODO(crbug.com/886583): pass correct |max_length|.
  base::string16 generatedPassword =
      _passwordGenerationHelper->GeneratePassword(
          [self lastCommittedURL], autofill::FormSignature(0),
          autofill::FieldSignature(0), /*max_length=*/0);

  self.generatedPotentialPassword = SysUTF16ToNSString(generatedPassword);

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(updateGeneratePasswordStrings:)
             name:UIContentSizeCategoryDidChangeNotification
           object:nil];

  // TODO(crbug.com/886583): add eg tests
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:@""
                         message:@""
                            rect:self.baseViewController.view.frame
                            view:self.baseViewController.view];
  self.actionSheetCoordinator.popoverArrowDirection = 0;
  self.actionSheetCoordinator.alertStyle =
      IsIPadIdiom() ? UIAlertControllerStyleAlert
                    : UIAlertControllerStyleActionSheet;

  // Set attributed text.
  [self updateGeneratePasswordStrings:self];

  __weak PasswordController* weakSelf = self;

  auto popupDismissed = ^{
    [weakSelf generatePasswordPopupDismissed];
  };

  [self.actionSheetCoordinator
      addItemWithTitle:GetNSString(IDS_IOS_USE_SUGGESTED_PASSWORD)
                action:^{
                  [weakSelf
                      injectGeneratedPasswordForFormName:formName
                                       generatedPassword:
                                           weakSelf.generatedPotentialPassword
                                       completionHandler:popupDismissed];
                }
                 style:UIAlertActionStyleDefault];

  [self.actionSheetCoordinator addItemWithTitle:GetNSString(IDS_CANCEL)
                                         action:popupDismissed
                                          style:UIAlertActionStyleCancel];

  // Set 'suggest' as preferred action, as per UX.
  self.actionSheetCoordinator.alertController.preferredAction =
      self.actionSheetCoordinator.alertController.actions[0];

  [self.actionSheetCoordinator start];
}

- (void)generatePasswordPopupDismissed {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  self.generatedPotentialPassword = nil;
}

- (void)updateGeneratePasswordStrings:(id)sender {
  NSString* title = [NSString
      stringWithFormat:@"%@\n%@\n ", GetNSString(IDS_IOS_SUGGESTED_PASSWORD),
                       self.generatedPotentialPassword];
  self.actionSheetCoordinator.attributedTitle =
      [[NSMutableAttributedString alloc]
          initWithString:title
              attributes:@{
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
              }];

  NSString* message = GetNSString(IDS_IOS_SUGGESTED_PASSWORD_HINT);
  self.actionSheetCoordinator.attributedMessage =
      [[NSMutableAttributedString alloc]
          initWithString:message
              attributes:@{
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
              }];

  // TODO(crbug.com/886583): find a way to make action sheet coordinator
  // responsible for font size changes.
  [self.actionSheetCoordinator updateAttributedText];
}

- (void)injectGeneratedPasswordForFormName:(NSString*)formName
                         generatedPassword:(NSString*)generatedPassword
                         completionHandler:(void (^)())completionHandler {
  const autofill::PasswordFormGenerationData* generation_data =
      [self getFormForGenerationFromFormName:formName];
  if (!generation_data)
    return;
  NSString* newPasswordIdentifier =
      SysUTF16ToNSString(generation_data->new_password_element);
  NSString* confirmPasswordIdentifier =
      SysUTF16ToNSString(generation_data->confirmation_password_element);

  auto generatedPasswordInjected = ^(BOOL success) {
    auto passwordPresaved = ^(BOOL found, const autofill::FormData& form) {
      if (found) {
        self.passwordManager->PresaveGeneratedPassword(
            self.passwordManagerDriver, form,
            SysNSStringToUTF16(generatedPassword),
            SysNSStringToUTF16(newPasswordIdentifier));
      }
      // If the form isn't found, it disappeared between fillPasswordForm below
      // and here. There isn't much that can be done.
    };
    if (success) {
      [self.formHelper extractPasswordFormData:formName
                             completionHandler:passwordPresaved];
      self.isPasswordGenerated = YES;
      self.passwordGeneratedIdentifier = newPasswordIdentifier;
    }
    if (completionHandler)
      completionHandler();
  };

  [self.formHelper fillPasswordForm:formName
              newPasswordIdentifier:newPasswordIdentifier
          confirmPasswordIdentifier:confirmPasswordIdentifier
                  generatedPassword:generatedPassword
                  completionHandler:generatedPasswordInjected];
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);

  if (!GetPageURLAndCheckTrustLevel(webState, nullptr))
    return;

  if (!frame || !frame->CanCallJavaScriptFunction())
    return;

  // Return early if |params| is not complete or if forms are not changed.
  if (params.input_missing || params.type != "form_changed")
    return;

  // If there's a change in password forms on a page, they should be parsed
  // again.
  [self findPasswordFormsAndSendThemToPasswordStore];
}

@end
