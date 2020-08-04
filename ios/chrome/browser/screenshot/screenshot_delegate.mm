// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screenshot/screenshot_delegate.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ScreenshotDelegate ()
@property(nonatomic, strong) id<BrowserInterfaceProvider>
    browserInterfaceProvider;
@end

@implementation ScreenshotDelegate

- (instancetype)initWithBrowserInterfaceProvider:
    (id<BrowserInterfaceProvider>)browserInterfaceProvider {
  self = [super init];
  if (self) {
    self.browserInterfaceProvider = browserInterfaceProvider;
  }
  return self;
}

#pragma mark - UIScreenshotServiceDelegate

// When there are multiple windows in the foreground UIKit will ask each
// UIScreenshotServiceDelegate for the PDF data and will display the PDF data of
// the widest window in the foreground.
- (void)screenshotService:(UIScreenshotService*)screenshotService
    generatePDFRepresentationWithCompletion:
        (void (^)(NSData*, NSInteger, CGRect))completionHandler
    API_AVAILABLE(ios(13.0)) {
  Browser* browser = [self.browserInterfaceProvider.currentInterface browser];

  if (!browser) {
    completionHandler(nil, 0, CGRectZero);
    return;
  }

  web::WebState* webState = browser->GetWebStateList()->GetActiveWebState();

  if (!webState) {
    completionHandler(nil, 0, CGRectZero);
    return;
  }

  base::OnceCallback<void(NSData*)> callback =
      base::BindOnce(^(NSData* pdfDoumentData) {
        completionHandler(pdfDoumentData, 0, CGRectZero);
      });

  webState->CreateFullPagePdf(std::move(callback));
}

@end
