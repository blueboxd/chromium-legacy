// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_view/wk_web_view_util.h"

#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

bool IsSafeBrowsingWarningDisplayedInWebView(WKWebView* web_view) {
  // A SafeBrowsing warning is a UIScrollView that is inserted on top of
  // WKWebView's scroll view. This method uses heuristics to detect this view.
  // It may break in the future if WebKit's implementation of SafeBrowsing
  // warnings changes.
  UIView* containing_view = web_view.scrollView.superview;
  if (!containing_view)
    return false;

  UIView* top_view = containing_view.subviews.lastObject;

  if (top_view == web_view.scrollView)
    return false;

  return [top_view isKindOfClass:[UIScrollView class]] &&
         [NSStringFromClass([top_view class]) containsString:@"Warning"] &&
         top_view.subviews.count > 0 &&
         [top_view.subviews.firstObject.subviews.lastObject
             isKindOfClass:[UIButton class]];
}

bool RequiresContentFilterBlockingWorkaround() {
  // This is fixed in iOS13 beta 7.
  if (@available(iOS 13, *))
    return false;

  if (@available(iOS 12.2, *))
    return true;

  return false;
}

bool RequiresProvisionalNavigationFailureWorkaround() {
  if (@available(iOS 12.2, *))
    return true;
  return false;
}

void CreateFullPagePdf(WKWebView* web_view,
                       base::OnceCallback<void(NSData*)> callback) {

  if (!web_view) {
    std::move(callback).Run(nil);
    return;
  }

  __block base::OnceCallback<void(NSData*)> callback_for_block =
      std::move(callback);

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  if (@available(iOS 14, *)) {
    WKPDFConfiguration* pdf_configuration = [[WKPDFConfiguration alloc] init];
    [web_view createPDFWithConfiguration:pdf_configuration
                       completionHandler:^(NSData* pdf_document_data,
                                           NSError* error) {
                         std::move(callback_for_block).Run(pdf_document_data);
                       }];
    return;
  }
#endif

  UIPrintPageRenderer* print_renderer = [[UIPrintPageRenderer alloc] init];
  [print_renderer addPrintFormatter:[web_view viewPrintFormatter]
              startingAtPageAtIndex:0];

  // Set the size of a page to be the size of the WebPage.
  CGRect entire_web_page =
      CGRectMake(0, 0, web_view.scrollView.contentSize.width,
                 web_view.scrollView.contentSize.height);
  [print_renderer setValue:[NSValue valueWithCGRect:entire_web_page]
                    forKey:@"paperRect"];
  [print_renderer setValue:[NSValue valueWithCGRect:entire_web_page]
                    forKey:@"printableRect"];

  UIGraphicsPDFRenderer* pdf_renderer =
      [[UIGraphicsPDFRenderer alloc] initWithBounds:entire_web_page];

  dispatch_async(dispatch_get_main_queue(), ^{
    NSData* pdf_document_data = [pdf_renderer
        PDFDataWithActions:^(UIGraphicsPDFRendererContext* context) {
          [context beginPage];
          [print_renderer drawPageAtIndex:0 inRect:entire_web_page];
        }];
    std::move(callback_for_block).Run(pdf_document_data);
  });
}
}  // namespace web
