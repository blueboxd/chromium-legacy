// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/remote_suggestions_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/sync/base/time.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

void AddVariationHeaders(network::ResourceRequest* request) {
  // Add Chrome experiment state to the request headers.
  //
  // Note: It's OK to pass InIncognito::kNo since we are expected to be in
  // non-incognito state here (i.e. remote sugestions are not served in
  // incognito mode).
  variations::AppendVariationsHeaderUnknownSignedIn(
      request->url, variations::InIncognito::kNo, request);
}

// Returns API request body. The final result depends on the following input
// variables:
//     * <current_url>: The current url visited by the user.
//     * <experiment_id>: the experiment id associated with the current field
//       trial group.
//
// The format of the request body is:
//
//     urls: {
//       url : <current_url>
//       // timestamp_usec is the timestamp for the page visit time, measured
//       // in microseconds since the Unix epoch.
//       timestamp_usec: <visit_time>
//     }
//     // stream_type = 1 corresponds to zero suggest suggestions.
//     stream_type: 1
//     // experiment_id is only set when <experiment_id> is well defined.
//     experiment_id: <experiment_id>
//
std::string FormatRequestBodyExperimentalService(const std::string& current_url,
                                                 const base::Time& visit_time) {
  auto request = std::make_unique<base::DictionaryValue>();
  auto url_list = std::make_unique<base::ListValue>();
  auto url_entry = std::make_unique<base::DictionaryValue>();
  url_entry->SetString("url", current_url);
  url_entry->SetString(
      "timestamp_usec",
      base::NumberToString(
          (visit_time - base::Time::UnixEpoch()).InMicroseconds()));
  url_list->Append(std::move(url_entry));
  request->Set("urls", std::move(url_list));
  // stream_type = 1 corresponds to zero suggest suggestions.
  request->SetInteger("stream_type", 1);
  const int experiment_id =
      OmniboxFieldTrial::GetOnFocusSuggestionsCustomEndpointExperimentId();
  if (experiment_id >= 0)
    request->SetInteger("experiment_id", experiment_id);
  std::string result;
  base::JSONWriter::Write(*request, &result);
  return result;
}

}  // namespace

RemoteSuggestionsService::RemoteSuggestionsService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      token_fetcher_(nullptr) {
  DCHECK(url_loader_factory);
}

RemoteSuggestionsService::~RemoteSuggestionsService() {}

void RemoteSuggestionsService::CreateSuggestionsRequest(
    const TemplateURLRef::SearchTermsArgs& search_terms_args,
    const base::Time& visit_time,
    const TemplateURLService* template_url_service,
    StartCallback start_callback,
    CompletionCallback completion_callback) {
  const GURL experimental_suggest_url = ExperimentalEndpointUrl(
      search_terms_args.current_page_url, template_url_service);
  if (experimental_suggest_url.is_valid())
    CreateExperimentalRequest(search_terms_args.current_page_url, visit_time,
                              experimental_suggest_url,
                              std::move(start_callback),
                              std::move(completion_callback));
  else
    CreateDefaultRequest(search_terms_args, template_url_service,
                         std::move(start_callback),
                         std::move(completion_callback));
}

void RemoteSuggestionsService::StopCreatingSuggestionsRequest() {
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      token_fetcher_deleter(std::move(token_fetcher_));
}

// static
GURL RemoteSuggestionsService::EndpointUrl(
    TemplateURLRef::SearchTermsArgs search_terms_args,
    const TemplateURLService* template_url_service) {
  if (template_url_service == nullptr) {
    return GURL();
  }

  const TemplateURL* search_engine =
      template_url_service->GetDefaultSearchProvider();
  if (search_engine == nullptr) {
    return GURL();
  }

  const TemplateURLRef& suggestion_url_ref =
      search_engine->suggestions_url_ref();
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();

  // Append a specific suggest client in ChromeOS app_list launcher contexts.
  BaseSearchProvider::AppendSuggestClientToAdditionalQueryParams(
      search_engine, search_terms_data, search_terms_args.page_classification,
      &search_terms_args);
  return GURL(suggestion_url_ref.ReplaceSearchTerms(search_terms_args,
                                                    search_terms_data));
}

GURL RemoteSuggestionsService::ExperimentalEndpointUrl(
    const std::string& current_url,
    const TemplateURLService* template_url_service) const {
  if (current_url.empty() || template_url_service == nullptr) {
    return GURL();
  }

  // An empty custom endpoint URL parameter indicates the service should
  // use the default endpoint, which is the default search provider.
  const std::string server_address_param =
      OmniboxFieldTrial::GetOnFocusSuggestionsCustomEndpointURL();
  if (server_address_param.empty())
    return GURL();

  // Check that the default search engine is Google.
  const TemplateURL& default_provider_url =
      *template_url_service->GetDefaultSearchProvider();
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();
  if (default_provider_url.GetEngineType(search_terms_data) !=
      SEARCH_ENGINE_GOOGLE) {
    return GURL();
  }

  GURL suggest_url(server_address_param);
  // Check that the custom endpoint URL is valid.
  if (!suggest_url.is_valid()) {
    return GURL();
  }

  // Check that the custom endpoint URL is HTTPS.
  if (!suggest_url.SchemeIsCryptographic()) {
    return GURL();
  }

  return suggest_url;
}

void RemoteSuggestionsService::CreateDefaultRequest(
    const TemplateURLRef::SearchTermsArgs& search_terms_args,
    const TemplateURLService* template_url_service,
    StartCallback start_callback,
    CompletionCallback completion_callback) {
  const GURL suggest_url = EndpointUrl(search_terms_args, template_url_service);
  DCHECK(suggest_url.is_valid());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_zerosuggest", R"(
        semantics {
          sender: "Omnibox"
          description:
            "When the user focuses the omnibox, Chrome can provide search or "
            "navigation suggestions from the default search provider in the "
            "omnibox dropdown, based on the current page URL.\n"
            "This is limited to users whose default search engine is Google, "
            "as no other search engines currently support this kind of "
            "suggestion."
          trigger: "The omnibox receives focus."
          data: "The URL of the current page."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature via the 'Use a prediction service "
            "to help complete searches and URLs typed in the address bar' "
            "settings under 'Privacy'. The feature is enabled by default."
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
          }
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = suggest_url;
  request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
  // Try to attach cookies for signed in user.
  request->attach_same_site_cookies = true;
  request->site_for_cookies = suggest_url;
  AddVariationHeaders(request.get());
  StartDownloadAndTransferLoader(std::move(request), std::string(),
                                 traffic_annotation, std::move(start_callback),
                                 std::move(completion_callback));
}

void RemoteSuggestionsService::CreateExperimentalRequest(
    const std::string& current_url,
    const base::Time& visit_time,
    const GURL& suggest_url,
    StartCallback start_callback,
    CompletionCallback completion_callback) {
  DCHECK(suggest_url.is_valid());

  // This traffic annotation is nearly identical to the annotation for
  // `omnibox_zerosuggest`. The main difference is that the experimental traffic
  // is not allowed cookies.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_zerosuggest_experimental",
                                          R"(
        semantics {
          sender: "Omnibox"
          description:
            "When the user focuses the omnibox, Chrome can provide search or "
            "navigation suggestions from the default search provider in the "
            "omnibox dropdown, based on the current page URL.\n"
            "This is limited to users whose default search engine is Google, "
            "as no other search engines currently support this kind of "
            "suggestion."
          trigger: "The omnibox receives focus."
          data: "The user's OAuth2 credentials and the URL of the current page."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature via the 'Use a prediction service "
            "to help complete searches and URLs typed in the address bar' "
            "settings under 'Privacy'. The feature is enabled by default."
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
          }
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = suggest_url;
  request->method = "POST";
  std::string request_body =
      FormatRequestBodyExperimentalService(current_url, visit_time);
  AddVariationHeaders(request.get());
  request->allow_credentials = false;

  // If authentication services are unavailable or if this request is still
  // waiting for an oauth2 token, run the remote service without access
  // tokens.
  if ((identity_manager_ == nullptr) || (token_fetcher_ != nullptr)) {
    StartDownloadAndTransferLoader(
        std::move(request), std::move(request_body), traffic_annotation,
        std::move(start_callback), std::move(completion_callback));
    return;
  }

  // As explained in http://crbug.com/968923, we have seen calls to fetch access
  // tokens for cusco-chrome-extension scope even when remote suggestion
  // feature was supposed to be disabled. This has caused a server issue for
  // GSuite users that were using sync. Adding a DCHECK to make sure that this
  // code is indeed never called when the custom endpoint URL is not configured.
  DCHECK(!OmniboxFieldTrial::GetOnFocusSuggestionsCustomEndpointURL().empty());

  // Create the oauth2 token fetcher.
  const identity::ScopeSet scopes{
      "https://www.googleapis.com/auth/cusco-chrome-extension"};
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "remote_suggestions_service", identity_manager_, scopes,
      base::BindOnce(&RemoteSuggestionsService::AccessTokenAvailable,
                     base::Unretained(this), std::move(request),
                     std::move(request_body), traffic_annotation,
                     std::move(start_callback), std::move(completion_callback)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

void RemoteSuggestionsService::AccessTokenAvailable(
    std::unique_ptr<network::ResourceRequest> request,
    std::string request_body,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    StartCallback start_callback,
    CompletionCallback completion_callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(token_fetcher_);
  token_fetcher_.reset();

  // If there were no errors obtaining the access token, append it to the
  // request as a header.
  if (error.state() == GoogleServiceAuthError::NONE) {
    DCHECK(!access_token_info.token.empty());
    request->headers.SetHeader(
        "Authorization",
        base::StringPrintf("Bearer %s", access_token_info.token.c_str()));
  }

  StartDownloadAndTransferLoader(std::move(request), std::move(request_body),
                                 traffic_annotation, std::move(start_callback),
                                 std::move(completion_callback));
}

void RemoteSuggestionsService::StartDownloadAndTransferLoader(
    std::unique_ptr<network::ResourceRequest> request,
    std::string request_body,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    StartCallback start_callback,
    CompletionCallback completion_callback) {
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  if (!request_body.empty()) {
    loader->AttachStringForUpload(request_body, "application/json");
  }
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(std::move(completion_callback), loader.get()));

  std::move(start_callback).Run(std::move(loader));
}
