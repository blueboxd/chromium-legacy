// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/search_engines/util.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/net/variations_http_headers.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/badges/badge_mediator.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_mediator.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#include "ios/chrome/browser/ui/location_bar/location_bar_view_controller.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"
#include "ios/chrome/browser/ui/omnibox/web_omnibox_edit_controller_impl.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#import "ios/chrome/browser/url_loading/url_loading_util.h"
#import "ios/chrome/browser/web/web_navigation_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state/web_state.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The histogram recording CLAuthorizationStatus for omnibox queries.
const char* const kOmniboxQueryLocationAuthorizationStatusHistogram =
    "Omnibox.QueryIosLocationAuthorizationStatus";
// The number of possible CLAuthorizationStatus values to report.
const int kLocationAuthorizationStatusCount = 5;
}  // namespace

@interface LocationBarCoordinator ()<LoadQueryCommands,
                                     LocationBarDelegate,
                                     LocationBarViewControllerDelegate,
                                     LocationBarConsumer> {
  // API endpoint for omnibox.
  std::unique_ptr<WebOmniboxEditControllerImpl> _editController;
  // Observer that updates |viewController| for fullscreen events.
  std::unique_ptr<FullscreenControllerObserver> _fullscreenObserver;
}
// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// Coordinator for the omnibox popup.
@property(nonatomic, strong) OmniboxPopupCoordinator* omniboxPopupCoordinator;
// Mediator for the badges displayed in the LocationBar.
@property(nonatomic, strong) BadgeMediator* badgeMediator;
// Coordinator for the omnibox.
@property(nonatomic, strong) OmniboxCoordinator* omniboxCoordinator;
@property(nonatomic, strong) LocationBarMediator* mediator;
@property(nonatomic, strong) LocationBarViewController* viewController;

// Tracks calls in progress to -cancelOmniboxEdit to avoid calling it from
// itself when -resignFirstResponder causes -textFieldWillResignFirstResponder
// delegate call.
@property(nonatomic, assign) BOOL isCancellingOmniboxEdit;

@end

@implementation LocationBarCoordinator
@synthesize commandDispatcher = _commandDispatcher;
@synthesize viewController = _viewController;
@synthesize started = _started;
@synthesize mediator = _mediator;
@synthesize browserState = _browserState;
@synthesize dispatcher = _dispatcher;
@synthesize delegate = _delegate;
@synthesize webStateList = _webStateList;
@synthesize omniboxPopupCoordinator = _omniboxPopupCoordinator;
@synthesize omniboxCoordinator = _omniboxCoordinator;

#pragma mark - public

- (UIViewController*)locationBarViewController {
  return self.viewController;
}

- (void)start {
  DCHECK(self.commandDispatcher);

  if (self.started)
    return;

  [self.commandDispatcher startDispatchingToTarget:self
                                       forProtocol:@protocol(OmniboxFocuser)];
  [self.commandDispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(LoadQueryCommands)];

  BOOL isIncognito = self.browserState->IsOffTheRecord();

  self.viewController = [[LocationBarViewController alloc] init];
  self.viewController.incognito = isIncognito;
  self.viewController.delegate = self;
  self.viewController.dispatcher =
      static_cast<id<ActivityServiceCommands, BrowserCommands,
                     ApplicationCommands, LoadQueryCommands>>(self.dispatcher);
  self.viewController.voiceSearchEnabled = ios::GetChromeBrowserProvider()
                                               ->GetVoiceSearchProvider()
                                               ->IsVoiceSearchEnabled();

  _editController = std::make_unique<WebOmniboxEditControllerImpl>(self);
  _editController->SetURLLoader(self);

  self.omniboxCoordinator = [[OmniboxCoordinator alloc] init];
  self.omniboxCoordinator.editController = _editController.get();
  self.omniboxCoordinator.browserState = self.browserState;
  self.omniboxCoordinator.dispatcher = self.dispatcher;
  [self.omniboxCoordinator start];

  [self.omniboxCoordinator.managedViewController
      willMoveToParentViewController:self.viewController];
  [self.viewController
      addChildViewController:self.omniboxCoordinator.managedViewController];
  [self.viewController
      setEditView:self.omniboxCoordinator.managedViewController.view];
  [self.omniboxCoordinator.managedViewController
      didMoveToParentViewController:self.viewController];
  self.viewController.offsetProvider = [self.omniboxCoordinator offsetProvider];

  self.omniboxPopupCoordinator = [self.omniboxCoordinator
      createPopupCoordinator:self.popupPresenterDelegate];
  self.omniboxPopupCoordinator.dispatcher = self.dispatcher;
  self.omniboxPopupCoordinator.webStateList = self.webStateList;
  [self.omniboxPopupCoordinator start];

  // Create BadgeMediator and set the viewController as its consumer.
  if (IsInfobarUIRebootEnabled()) {
    self.badgeMediator =
        [[BadgeMediator alloc] initWithConsumer:self.viewController
                                   webStateList:self.webStateList];
  }

  self.mediator = [[LocationBarMediator alloc]
      initWithLocationBarModel:[self locationBarModel]];
  self.mediator.webStateList = self.webStateList;
  self.mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(self.browserState);
  self.mediator.consumer = self;

  _fullscreenObserver =
      std::make_unique<FullscreenUIUpdater>(self.viewController);
  FullscreenControllerFactory::GetInstance()
      ->GetForBrowserState(self.browserState)
      ->AddObserver(_fullscreenObserver.get());

  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  [self.commandDispatcher stopDispatchingToTarget:self];
  // The popup has to be destroyed before the location bar.
  [self.omniboxPopupCoordinator stop];
  [self.omniboxCoordinator stop];
  [self.badgeMediator disconnect];
  _editController.reset();

  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;

  FullscreenControllerFactory::GetInstance()
      ->GetForBrowserState(self.browserState)
      ->RemoveObserver(_fullscreenObserver.get());
  self.started = NO;
}

- (BOOL)omniboxPopupHasAutocompleteResults {
  return self.omniboxPopupCoordinator.hasResults;
}

- (BOOL)showingOmniboxPopup {
  return self.omniboxPopupCoordinator.isOpen;
}

- (BOOL)isOmniboxFirstResponder {
  return [self.omniboxCoordinator isOmniboxFirstResponder];
}

- (id<LocationBarAnimatee>)locationBarAnimatee {
  return self.viewController;
}

- (id<EditViewAnimatee>)editViewAnimatee {
  return self.omniboxCoordinator.animatee;
}

#pragma mark - LoadQueryCommands

- (void)loadQuery:(NSString*)query immediately:(BOOL)immediately {
  DCHECK(query);
  // Since the query is not user typed, sanitize it to make sure it's safe.
  base::string16 sanitizedQuery =
      OmniboxView::SanitizeTextForPaste(base::SysNSStringToUTF16(query));
  if (immediately) {
    [self loadURLForQuery:sanitizedQuery];
  } else {
    [self.omniboxCoordinator focusOmnibox];
    [self.omniboxCoordinator
        insertTextToOmnibox:base::SysUTF16ToNSString(sanitizedQuery)];
  }
}

#pragma mark - LocationBarURLLoader

- (void)loadGURLFromLocationBar:(const GURL&)url
                    postContent:(TemplateURLRef::PostContent*)postContent
                     transition:(ui::PageTransition)transition
                    disposition:(WindowOpenDisposition)disposition {
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    LoadJavaScriptURL(url, self.browserState,
                      self.webStateList->GetActiveWebState());
  } else {
    // When opening a URL, warn the omnibox geolocation in case it needs to stop
    // the service.
    [[OmniboxGeolocationController sharedInstance] locationBarDidSubmitURL];

    // TODO(crbug.com/785244): Is it ok to call |cancelOmniboxEdit| after
    // |loadURL|?  It doesn't seem to be causing major problems.  If we call
    // cancel before load, then any prerendered pages get destroyed before the
    // call to load.
    web::NavigationManager::WebLoadParams web_params =
        web_navigation_util::CreateWebLoadParams(url, transition, postContent);
    NSMutableDictionary* combinedExtraHeaders =
        [[self variationHeadersForURL:url] mutableCopy];
    [combinedExtraHeaders addEntriesFromDictionary:web_params.extra_headers];
    web_params.extra_headers = [combinedExtraHeaders copy];
    UrlLoadParams params = UrlLoadParams::InCurrentTab(web_params);
    params.disposition = disposition;
    UrlLoadingServiceFactory::GetForBrowserState(self.browserState)
        ->Load(params);

    if (google_util::IsGoogleSearchUrl(url)) {
      UMA_HISTOGRAM_ENUMERATION(
          kOmniboxQueryLocationAuthorizationStatusHistogram,
          [CLLocationManager authorizationStatus],
          kLocationAuthorizationStatusCount);
    }
  }
  [self cancelOmniboxEdit];
}

#pragma mark - OmniboxFocuser

- (void)focusOmniboxFromSearchButton {
  // TODO(crbug.com/931284): Temporary workaround for intermediate broken state
  // in the NTP.  Remove this once crbug.com/899827 is fixed.
  if (self.webState) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(self.webState);
    if (NTPHelper && NTPHelper->IsActive() && NTPHelper->IgnoreLoadRequests()) {
      return;
    }
  }
  [self.omniboxCoordinator setNextFocusSourceAsSearchButton];
  [self focusOmnibox];
}

- (void)focusOmniboxFromFakebox {
  [self.omniboxCoordinator focusOmnibox];
}

- (void)focusOmnibox {
  // TODO(crbug.com/931284): Temporary workaround for intermediate broken state
  // in the NTP.  Remove this once crbug.com/899827 is fixed.
  if (self.webState) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(self.webState);
    if (NTPHelper && NTPHelper->IsActive() && NTPHelper->IgnoreLoadRequests()) {
      return;
    }
  }
  // Dismiss the edit menu.
  [[UIMenuController sharedMenuController] setMenuVisible:NO animated:NO];

  // When the NTP and fakebox are visible, make the fakebox animates into place
  // before focusing the omnibox.
  if (IsVisibleURLNewTabPage([self webState]) &&
      !self.browserState->IsOffTheRecord()) {
    [self.viewController.dispatcher focusFakebox];
  } else {
    [self.omniboxCoordinator focusOmnibox];
    [self.omniboxPopupCoordinator presentShortcutsIfNecessary];
  }
}

- (void)cancelOmniboxEdit {
  if (self.isCancellingOmniboxEdit) {
    return;
  }
  self.isCancellingOmniboxEdit = YES;
  [self.omniboxCoordinator endEditing];
  [self.omniboxPopupCoordinator dismissShortcuts];
  self.isCancellingOmniboxEdit = NO;
}

#pragma mark - LocationBarDelegate

- (void)locationBarHasBecomeFirstResponder {
  [self.delegate locationBarDidBecomeFirstResponder];
}

- (void)locationBarHasResignedFirstResponder {
  [self.delegate locationBarDidResignFirstResponder];
}

- (void)locationBarBeganEdit {
  [self.delegate locationBarBeganEdit];
}

- (web::WebState*)webState {
  return self.webStateList->GetActiveWebState();
}

- (LocationBarModel*)locationBarModel {
  return [self.delegate locationBarModel];
}

#pragma mark - LocationBarViewControllerDelegate

- (void)locationBarSteadyViewTapped {
  [self focusOmnibox];
}

- (void)locationBarCopyTapped {
  StoreURLInPasteboard(self.webState->GetVisibleURL());
}

#pragma mark - LocationBarConsumer

- (void)updateLocationText:(NSString*)text clipTail:(BOOL)clipTail {
  [self.omniboxCoordinator updateOmniboxState];
  [self.viewController updateLocationText:text clipTail:clipTail];
  [self.viewController updateForNTP:NO];
}

- (void)defocusOmnibox {
  [self cancelOmniboxEdit];
}

- (void)updateLocationIcon:(UIImage*)icon
        securityStatusText:(NSString*)statusText {
  [self.viewController updateLocationIcon:icon securityStatusText:statusText];
}

- (void)updateAfterNavigatingToNTP {
  [self.viewController updateForNTP:YES];
}

- (void)updateLocationShareable:(BOOL)shareable {
  [self.viewController setShareButtonEnabled:shareable];
}

- (void)updateSearchByImageSupported:(BOOL)searchByImageSupported {
  self.viewController.searchByImageEnabled = searchByImageSupported;
}

- (void)displayInfobarBadge:(BOOL)display type:(InfobarType)infobarType {
  InfobarMetricsRecorder* metricsRecorder;
  // If the Badge will be displayed create a metrics recorder to log its
  // interactions, if its hidden metrics recorder should be nil.
  if (display)
    metricsRecorder = [[InfobarMetricsRecorder alloc] initWithType:infobarType];

  [self.viewController displayInfobarButton:display
                            metricsRecorder:metricsRecorder];
}

- (void)activeInfobarBadge:(BOOL)active {
  [self.viewController setInfobarButtonStyleActive:active];
}

#pragma mark - private

// Returns a dictionary with variation headers for qualified URLs. Can be empty.
- (NSDictionary*)variationHeadersForURL:(const GURL&)URL {
  network::ResourceRequest resource_request;
  variations::AppendVariationsHeaderUnknownSignedIn(
      URL,
      self.browserState->IsOffTheRecord() ? variations::InIncognito::kYes
                                          : variations::InIncognito::kNo,
      &resource_request);
  NSMutableDictionary* result = [NSMutableDictionary dictionary];
  // The variations header appears in cors_exempt_headers rather than in
  // headers.
  net::HttpRequestHeaders::Iterator header_iterator(
      resource_request.cors_exempt_headers);
  while (header_iterator.GetNext()) {
    NSString* name = base::SysUTF8ToNSString(header_iterator.name());
    NSString* value = base::SysUTF8ToNSString(header_iterator.value());
    result[name] = value;
  }
  return [result copy];
}

// Navigate to |query| from omnibox.
- (void)loadURLForQuery:(const base::string16&)query {
  GURL searchURL;
  metrics::OmniboxInputType type = AutocompleteInput::Parse(
      query, std::string(), AutocompleteSchemeClassifierImpl(), nullptr,
      nullptr, &searchURL);
  if (type != metrics::OmniboxInputType::URL || !searchURL.is_valid()) {
    searchURL = GetDefaultSearchURLForSearchTerms(
        ios::TemplateURLServiceFactory::GetForBrowserState(self.browserState),
        query);
  }
  if (searchURL.is_valid()) {
    // It is necessary to include PAGE_TRANSITION_FROM_ADDRESS_BAR in the
    // transition type is so that query-in-the-omnibox is triggered for the
    // URL.
    UrlLoadParams params = UrlLoadParams::InCurrentTab(searchURL);
    params.web_params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    UrlLoadingServiceFactory::GetForBrowserState(self.browserState)
        ->Load(params);
  }
}

@end
