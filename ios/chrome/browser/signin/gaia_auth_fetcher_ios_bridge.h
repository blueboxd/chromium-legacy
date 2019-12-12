// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_BRIDGE_H_
#define IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_BRIDGE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

class GURL;
@class NSURLRequest;

namespace web {
class BrowserState;
}

// Specialization of GaiaAuthFetcher on iOS.
//
// Authenticate a user against the Google Accounts ClientLogin API
// with various capabilities and return results to a GaiaAuthConsumer.
// This class is an interface, and FetchPendingRequest() and Cancel() have to
// be implemented using native APIs in a subclass.
class GaiaAuthFetcherIOSBridge {
 public:
  // Delegate class receive notification whent the request is done.
  class GaiaAuthFetcherIOSBridgeDelegate {
   public:
    GaiaAuthFetcherIOSBridgeDelegate();
    virtual ~GaiaAuthFetcherIOSBridgeDelegate();

    // Called when the request is done.
    virtual void OnFetchComplete(const GURL& url,
                                 const std::string& data,
                                 net::Error net_error,
                                 int response_code) = 0;

    DISALLOW_COPY_AND_ASSIGN(GaiaAuthFetcherIOSBridgeDelegate);
  };

  // Initializes the instance.
  GaiaAuthFetcherIOSBridge(GaiaAuthFetcherIOSBridgeDelegate* delegate,
                           web::BrowserState* browser_state);
  virtual ~GaiaAuthFetcherIOSBridge();

  // Starts a network fetch.
  // * |url| is the URL to fetch.
  // * |headers| are the HTTP headers to add to the request.
  // * |body| is the HTTP body to add to the request. If not empty, the fetch
  //   will be a POST request.
  void Fetch(const GURL& url,
             const std::string& headers,
             const std::string& body,
             bool should_use_xml_http_request);

  // Cancels the current fetch.
  virtual void Cancel() = 0;

  // Informs the bridge of the success of the URL fetch.
  // * |data| is the body of the HTTP response.
  // * |response_code| is the response code.
  // URLFetchSuccess and URLFetchFailure are no-op if one of them was already
  // called.
  void OnURLFetchSuccess(const std::string& data, int response_code);

  // Informs the bridge of the failure of the URL fetch.
  // * |is_cancelled| whether the fetch failed because it was cancelled.
  // URLFetchSuccess and URLFetchFailure are no-op if one of them was already
  // called.
  void OnURLFetchFailure(int error, int response_code);

  // Returns the current browser state.
  web::BrowserState* GetBrowserState() const;

 protected:
  // Fetches the pending request if it exists. The subclass needs to update the
  // cookie store for each redirect and call either URLFetchSuccess() or
  // URLFetchFailure().
  virtual void FetchPendingRequest() = 0;

  // A network request that needs to be fetched.
  struct Request {
    Request();
    Request(const GURL& url,
            const std::string& headers,
            const std::string& body,
            bool should_use_xml_http_request);
    // Whether the request is pending (i.e. awaiting to be processed or
    // currently being processed).
    bool pending;
    // URL to fetch.
    GURL url;
    // HTTP headers to add to the request.
    std::string headers;
    // HTTP body to add to the request.
    std::string body;
    // Whether XmlHTTPRequest should be injected in JS instead of using
    // WKWebView directly.
    bool should_use_xml_http_request;
  };

  // Returns a |request_| that contains the url, the headers and the body
  // received in the constructor of this instance.
  const Request& GetRequest() const;

  // Creates a NSURLRequest with the url, the headers and the body received in
  // the constructor of this instance. The request is a GET if |body| is empty
  // and a POST otherwise.
  NSURLRequest* GetNSURLRequest() const;

 private:
  // Finishes the pending request and cleans up its associated state. Returns
  // the URL of the request.
  GURL FinishPendingRequest();

  // Delegate.
  GaiaAuthFetcherIOSBridgeDelegate* delegate_;
  // Browser state associated with the bridge.
  web::BrowserState* browser_state_;
  // Request currently processed by the bridge.
  Request request_;

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthFetcherIOSBridge);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_BRIDGE_H_
