// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios_bridge.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#import "net/base/mac/url_conversions.h"
#include "net/http/http_request_headers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate

GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate::
    GaiaAuthFetcherIOSBridgeDelegate() {}

GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate::
    ~GaiaAuthFetcherIOSBridgeDelegate() {}

#pragma mark - GaiaAuthFetcherIOSBridge::Request

GaiaAuthFetcherIOSBridge::Request::Request()
    : pending(false), should_use_xml_http_request(false) {}

GaiaAuthFetcherIOSBridge::Request::Request(const GURL& request_url,
                                           const std::string& request_headers,
                                           const std::string& request_body,
                                           bool should_use_xml_http_request)
    : pending(true),
      url(request_url),
      headers(request_headers),
      body(request_body),
      should_use_xml_http_request(should_use_xml_http_request) {}

#pragma mark - GaiaAuthFetcherIOSBridge

GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridge(
    GaiaAuthFetcherIOSBridgeDelegate* delegate,
    web::BrowserState* browser_state)
    : delegate_(delegate), browser_state_(browser_state) {}

GaiaAuthFetcherIOSBridge::~GaiaAuthFetcherIOSBridge() {}

void GaiaAuthFetcherIOSBridge::Fetch(const GURL& url,
                                     const std::string& headers,
                                     const std::string& body,
                                     bool should_use_xml_http_request) {
  request_ = Request(url, headers, body, should_use_xml_http_request);
  FetchPendingRequest();
}

void GaiaAuthFetcherIOSBridge::OnURLFetchSuccess(const std::string& data,
                                                 int response_code) {
  if (!request_.pending) {
    return;
  }
  GURL url = FinishPendingRequest();
  delegate_->OnFetchComplete(url, data, net::OK, response_code);
}

void GaiaAuthFetcherIOSBridge::OnURLFetchFailure(int error, int response_code) {
  if (!request_.pending) {
    return;
  }
  GURL url = FinishPendingRequest();
  delegate_->OnFetchComplete(url, std::string(), static_cast<net::Error>(error),
                             response_code);
}

web::BrowserState* GaiaAuthFetcherIOSBridge::GetBrowserState() const {
  return browser_state_;
}

const GaiaAuthFetcherIOSBridge::Request& GaiaAuthFetcherIOSBridge::GetRequest()
    const {
  return request_;
}

NSURLRequest* GaiaAuthFetcherIOSBridge::GetNSURLRequest() const {
  NSMutableURLRequest* request = [[NSMutableURLRequest alloc]
      initWithURL:net::NSURLWithGURL(request_.url)];
  net::HttpRequestHeaders request_headers;
  request_headers.AddHeadersFromString(request_.headers);
  for (net::HttpRequestHeaders::Iterator it(request_headers); it.GetNext();) {
    [request setValue:base::SysUTF8ToNSString(it.value())
        forHTTPHeaderField:base::SysUTF8ToNSString(it.name())];
  }
  if (!request_.body.empty()) {
    NSData* post_data = [base::SysUTF8ToNSString(request_.body)
        dataUsingEncoding:NSUTF8StringEncoding];
    [request setHTTPBody:post_data];
    [request setHTTPMethod:@"POST"];
    DCHECK(![[request allHTTPHeaderFields] objectForKey:@"Content-Type"]);
    [request setValue:@"application/x-www-form-urlencoded"
        forHTTPHeaderField:@"Content-Type"];
  }
  return request;
}

GURL GaiaAuthFetcherIOSBridge::FinishPendingRequest() {
  GURL url = request_.url;
  request_ = Request();
  return url;
}
