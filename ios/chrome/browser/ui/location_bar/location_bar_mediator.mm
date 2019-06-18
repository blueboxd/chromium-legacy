// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/search_engines_util.h"
#include "ios/chrome/browser/ssl/ios_security_state_tab_helper.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_consumer.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "skia/ext/skia_utils_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LocationBarMediator () <CRWWebStateObserver,
                                   InfobarBadgeTabHelperDelegate,
                                   SearchEngineObserving,
                                   WebStateListObserving>

// The current web state associated with the toolbar.
@property(nonatomic, assign) web::WebState* webState;

// Whether the current default search engine supports search by image.
@property(nonatomic, assign) BOOL searchEngineSupportsSearchByImage;
@end

@implementation LocationBarMediator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
}
@synthesize badgeState = _badgeState;

- (instancetype)initWithLocationBarModel:(LocationBarModel*)locationBarModel {
  DCHECK(locationBarModel);
  self = [super init];
  if (self) {
    _locationBarModel = locationBarModel;
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _searchEngineSupportsSearchByImage = NO;
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - Public

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }

  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self notifyConsumerOfChangedLocation];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webState:(web::WebState*)webState
    didPruneNavigationItemsWithCount:(size_t)pruned_item_count {
  DCHECK_EQ(_webState, webState);
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  // Currently, because of https://crbug.com/448486 , interstitials are not
  // commited navigations. This means that if a security interstitial (e.g. for
  // broken HTTPS) is shown, didFinishNavigation: is not called, and
  // didChangeVisibleSecurityState: is the only chance to update the URL.
  // Otherwise it would be preferable to only update the icon here.
  [self notifyConsumerOfChangedLocation];

  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webState = nullptr;
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  DCHECK_EQ(_webStateList, webStateList);
  self.webState = newWebState;
  [self.consumer defocusOmnibox];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  self.searchEngineSupportsSearchByImage =
      search_engines::SupportsSearchByImage(self.templateURLService);
}

#pragma mark - InfobarBadgeTabHelper

- (void)displayBadge:(BOOL)display type:(InfobarType)infobarType {
  DCHECK(IsInfobarUIRebootEnabled());
  [self.consumer displayInfobarBadge:display type:infobarType];
}

- (void)setBadgeState:(InfobarBadgeState)badgeState {
  _badgeState = badgeState;
  [self.consumer activeInfobarBadge:_badgeState & InfobarBadgeStateAccepted];
}

#pragma mark - Setters

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
  }

  _webState = webState;

  if (_webState) {
    _webState->AddObserver(_webStateObserver.get());

    if (IsInfobarUIRebootEnabled()) {
      InfobarBadgeTabHelper* infobarBadgeTabHelper =
          InfobarBadgeTabHelper::FromWebState(_webState);
      DCHECK(infobarBadgeTabHelper);
      infobarBadgeTabHelper->SetDelegate(self);
      if (self.consumer) {
        // Whenever the WebState changes ask the corresponding
        // InfobarBadgeTabHelper if a badge should be displayed, and if its
        // Active or not.
        [self.consumer
            displayInfobarBadge:infobarBadgeTabHelper->is_infobar_displaying()
                           type:infobarBadgeTabHelper->infobar_type()];
        if (infobarBadgeTabHelper->is_badge_accepted()) {
          self.badgeState |= InfobarBadgeStateAccepted;
        } else {
          self.badgeState &= ~InfobarBadgeStateAccepted;
        }
      }
    }

    if (self.consumer) {
      [self notifyConsumerOfChangedLocation];
      [self notifyConsumerOfChangedSecurityIcon];
    }
  }
}

- (void)setConsumer:(id<LocationBarConsumer>)consumer {
  _consumer = consumer;
  if (self.webState) {
    [self notifyConsumerOfChangedLocation];
    [self notifyConsumerOfChangedSecurityIcon];
  }
  [consumer
      updateSearchByImageSupported:self.searchEngineSupportsSearchByImage];
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;
  self.webState = self.webStateList->GetActiveWebState();
  _webStateList->AddObserver(_webStateListObserver.get());
}

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  self.searchEngineSupportsSearchByImage =
      search_engines::SupportsSearchByImage(templateURLService);
  _searchEngineObserver =
      std::make_unique<SearchEngineObserverBridge>(self, templateURLService);
}

- (void)setSearchEngineSupportsSearchByImage:
    (BOOL)searchEngineSupportsSearchByImage {
  BOOL supportChanged =
      _searchEngineSupportsSearchByImage != searchEngineSupportsSearchByImage;
  _searchEngineSupportsSearchByImage = searchEngineSupportsSearchByImage;
  if (supportChanged) {
    [self.consumer
        updateSearchByImageSupported:searchEngineSupportsSearchByImage];
  }
}

#pragma mark - private

- (void)notifyConsumerOfChangedLocation {
  [self.consumer updateLocationText:[self currentLocationString]
                           clipTail:[self locationShouldClipTail]];
  GURL URL = self.webState->GetVisibleURL();
  BOOL isNTP = IsURLNewTabPage(URL);
  if (isNTP) {
    [self.consumer updateAfterNavigatingToNTP];
  }
  [self.consumer updateLocationShareable:[self isCurrentPageShareable]];
}

- (void)notifyConsumerOfChangedSecurityIcon {
  [self.consumer updateLocationIcon:[self currentLocationIcon]
                 securityStatusText:base::SysUTF16ToNSString(
                                        self.locationBarModel
                                            ->GetSecureAccessibilityText())];
}

#pragma mark Location helpers

- (NSString*)currentLocationString {
  base::string16 string = self.locationBarModel->GetURLForDisplay();
  return base::SysUTF16ToNSString(string);
}

// Some URLs (data://) should have their tail clipped when presented; while for
// others (http://) it would be more appropriate to clip the head.
- (BOOL)locationShouldClipTail {
  GURL url = self.locationBarModel->GetURL();
  return url.SchemeIs(url::kDataScheme);
}

#pragma mark Security status icon helpers

- (UIImage*)currentLocationIcon {
  if (!self.locationBarModel->ShouldDisplayURL()) {
    return nil;
  }

  if (self.locationBarModel->IsOfflinePage()) {
    return [self imageForOfflinePage];
  }

  return GetLocationBarSecurityIconForSecurityState(
      self.locationBarModel->GetSecurityLevel());
}

// Returns a location icon for offline pages.
- (UIImage*)imageForOfflinePage {
  return [UIImage imageNamed:@"location_bar_offline"];
}

#pragma mark Shareability helpers

- (BOOL)isCurrentPageShareable {
  const GURL& URL = self.webState->GetLastCommittedURL();
  return URL.is_valid() && !web::GetWebClient()->IsAppSpecificURL(URL);
}

@end
