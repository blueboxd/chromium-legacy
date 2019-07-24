// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/search_engines/template_url.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace base {
class Time;
}
namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin
class GoogleServiceAuthError;
class TemplateURLService;

namespace network {
struct ResourceRequest;
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// A service to fetch suggestions from a remote endpoint. Usually, the remote
// endpoint is the default search provider's suggest service. However, the
// endpoint URL can be customized by field trial.
//
// This service is always sent the user's authentication state, so the
// suggestions always can be personalized. This service is also sometimes sent
// the user's current URL, so the suggestions are sometimes also contextual.
class RemoteSuggestionsService : public KeyedService {
 public:
  // |identity_manager| may be null but only unauthenticated requests will
  // issued.
  RemoteSuggestionsService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~RemoteSuggestionsService() override;

  using StartCallback = base::OnceCallback<void(
      std::unique_ptr<network::SimpleURLLoader> loader)>;

  using CompletionCallback =
      base::OnceCallback<void(const network::SimpleURLLoader* source,
                              std::unique_ptr<std::string> response_body)>;

  // Creates a loader for remote suggestions for |search_terms_args| and passes
  // the loader to |start_callback|. It uses a number of signals to create the
  // loader, including field trial / experimental parameters, and it may
  // pass a nullptr to |start_callback| (see below for details).
  //
  // |search_terms_args| encapsulates the arguments sent to the remote service.
  // If |search_terms_args.current_page_url| is empty, the system will never use
  // the experimental suggestions service. It's possible the non-experimental
  // service may decide to offer general-purpose suggestions.
  //
  // |visit_time| is the time of the visit for the URL for which suggestions
  // should be fetched.
  //
  // |template_url_service| may be null, but some services may be disabled.
  //
  // |start_callback| is called to transfer ownership of the created loader to
  //  whatever function/class receives the callback.
  // |start_callback| is called with a nullptr argument if:
  //   * The service is waiting for a previously-requested authentication token.
  //   * The authentication token had any issues.
  //
  // |completion_callback| will be invoked when the transfer is done.
  //
  // This method sends a request for an OAuth2 token and temporarily
  // instantiates |token_fetcher_|.
  void CreateSuggestionsRequest(
      const TemplateURLRef::SearchTermsArgs& search_terms_args,
      const base::Time& visit_time,
      const TemplateURLService* template_url_service,
      StartCallback start_callback,
      CompletionCallback completion_callback);

  // Advises the service to stop any process that creates a suggestion request.
  void StopCreatingSuggestionsRequest();

  // Returns a URL representing the address of the server where the zero suggest
  // request is being sent. Does not take into account whether sending this
  // request is prohibited (e.g. in an incognito window).
  // Returns an invalid URL (i.e.: GURL::is_valid() == false) in case of an
  // error.
  //
  // |search_terms_args| encapsulates the arguments sent to the suggest service.
  // Various parts of it (including the current page URL and classification) are
  // used to build the final endpoint URL. Note that the current page URL can
  // be empty.
  //
  // Note that this method is public and is also used by ZeroSuggestProvider for
  // suggestions that do not take the current page URL into consideration.
  static GURL EndpointUrl(TemplateURLRef::SearchTermsArgs search_terms_args,
                          const TemplateURLService* template_url_service);

 private:
  // Returns a URL representing the address of the server where the zero suggest
  // requests are being redirected when serving experimental suggestions.
  //
  // Returns a valid URL only if all the folowing conditions are true:
  // * The |current_url| is non-empty.
  // * The |default_provider| is Google.
  // * The user is in the ZeroSuggestRedirectToChrome field trial.
  // * The field trial provides a valid server address where the suggest request
  //   is redirected.
  // * The suggest request is over HTTPS. This avoids leaking the current page
  //   URL or personal data in unencrypted network traffic.
  // Note: these checks are in addition to CanSendUrl() on the default
  // remote suggestion URL.
  GURL ExperimentalEndpointUrl(
      const std::string& current_url,
      const TemplateURLService* template_url_service) const;

  // Upon succesful creation of an HTTP GET request for default remote
  // suggestions, the |callback| function is run with the HTTP GET request as a
  // parameter.
  //
  // This function is called by CreateSuggestionsRequest. See its
  // function definition for details on the parameters.
  void CreateDefaultRequest(
      const TemplateURLRef::SearchTermsArgs& search_terms_args,
      const TemplateURLService* template_url_service,
      StartCallback start_callback,
      CompletionCallback completion_callback);

  // Upon succesful creation of an HTTP POST request for experimental remote
  // suggestions, the |callback| function is run with the HTTP POST request as a
  // parameter.
  //
  // This function is called by CreateSuggestionsRequest. See its
  // function definition for details on the parameters.
  void CreateExperimentalRequest(const std::string& current_url,
                                 const base::Time& visit_time,
                                 const GURL& suggest_url,
                                 StartCallback start_callback,
                                 CompletionCallback completion_callback);

  // Tries to load the suggestions from network, using the token when available.
  //
  // |request| is expected to be the request for suggestions filled in with
  // everything but maybe the authentication token and the request body.
  //
  // |request_body|, if not empty, will be attached as the upload body to
  // the request.
  //
  // |traffic_annotation| is the appropriate annotations for making the network
  // request to load the suggestions.
  //
  // |start_callback|, |completion_callback| are the same as passed to
  // CreateSuggestionsRequest()
  //
  // |error| and |access_token| are the results of token request.
  void AccessTokenAvailable(std::unique_ptr<network::ResourceRequest> request,
                            std::string request_body,
                            net::NetworkTrafficAnnotationTag traffic_annotation,
                            StartCallback start_callback,
                            CompletionCallback completion_callback,
                            GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);

  // Activates a loader for |request|, wiring it up to |completion_callback|,
  // and calls |start_callback|.  If |request_body| isn't empty, it will be
  // attached as upload bytes.
  void StartDownloadAndTransferLoader(
      std::unique_ptr<network::ResourceRequest> request,
      std::string request_body,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      StartCallback start_callback,
      CompletionCallback completion_callback);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  signin::IdentityManager* identity_manager_;

  // Helper for fetching OAuth2 access tokens. This is non-null when an access
  // token request is currently in progress.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsService);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
