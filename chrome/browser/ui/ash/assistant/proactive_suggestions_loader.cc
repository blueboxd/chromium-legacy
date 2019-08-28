// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/proactive_suggestions_loader.h"

#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("proactive_suggestions_loader", R"(
          semantics: {
            sender: "Google Assistant Proactive Suggestions"
            description:
              "The Google Assistant requests proactive content suggestions "
              "based on the currently active browser context."
            trigger:
              "Change of active browser context."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy: {
            cookies_allowed: YES
            setting:
              "The Google Assistant can be enabled/disabled in Chrome Settings "
              "and is subject to eligibility requirements. The user may also "
              "separately opt out of sharing screen context with Assistant."
          })");

// Helpers ---------------------------------------------------------------------

// Returns the url to retrieve proactive content suggestions for a given |url|.
GURL CreateProactiveSuggestionsUrl(const GURL& url) {
  static constexpr char kProactiveSuggestionsEndpoint[] =
      "https://assistant.google.com/proactivesuggestions/embeddedview";
  GURL result = GURL(kProactiveSuggestionsEndpoint);

  // The proactive suggestions service needs to be aware of the device locale.
  static constexpr char kLocaleParamKey[] = "hl";
  result = net::AppendOrReplaceQueryParameter(
      result, kLocaleParamKey, base::i18n::GetConfiguredLocale());

  // The proactive suggestions service needs to be informed of the given |url|.
  static constexpr char kUrlParamKey[] = "url";
  return net::AppendOrReplaceQueryParameter(result, kUrlParamKey, url.spec());
}

// Parses proactive suggestions metadata from the specified |headers|.
void ParseProactiveSuggestionsMetadata(const net::HttpResponseHeaders& headers,
                                       std::string* description,
                                       bool* has_content) {
  static constexpr char kDescriptionHeaderName[] =
      "x-assistant-proactive-suggestions-description";
  static constexpr char kHasContentHeaderName[] =
      "x-assistant-proactive-suggestions-has-ui-content";

  DCHECK(description->empty());
  DCHECK(!(*has_content));

  size_t it = 0;
  std::string name;
  std::string value;

  while (headers.EnumerateHeaderLines(&it, &name, &value)) {
    if (name == kDescriptionHeaderName) {
      DCHECK(description->empty());
      *description = value;
      continue;
    }
    if (name == kHasContentHeaderName) {
      DCHECK(!(*has_content));
      *has_content = (value == "1");
    }
  }
}

}  // namespace

// ProactiveSuggestionsLoader --------------------------------------------------

ProactiveSuggestionsLoader::ProactiveSuggestionsLoader(Profile* profile,
                                                       const GURL& url)
    : profile_(profile), url_(url) {}

ProactiveSuggestionsLoader::~ProactiveSuggestionsLoader() {
  if (complete_callback_)
    std::move(complete_callback_).Run(/*proactive_suggestions=*/nullptr);
}

void ProactiveSuggestionsLoader::Start(CompleteCallback complete_callback) {
  DCHECK(!loader_);
  complete_callback_ = std::move(complete_callback);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = CreateProactiveSuggestionsUrl(url_);

  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             kNetworkTrafficAnnotationTag);

  auto* url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get();

  auto simple_url_loader_complete_callback =
      base::BindOnce(&ProactiveSuggestionsLoader::OnSimpleURLLoaderComplete,
                     base::Unretained(this));

  loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory, std::move(simple_url_loader_complete_callback));
}

void ProactiveSuggestionsLoader::OnSimpleURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  // When the request fails to return a valid response we return a null set of
  // proactive suggestions.
  if (!response_body || loader_->NetError() != net::OK ||
      !loader_->ResponseInfo() || !loader_->ResponseInfo()->headers) {
    std::move(complete_callback_).Run(/*proactive_suggestions=*/nullptr);
    return;
  }

  // The proactive suggestions server will return metadata in the HTTP headers
  // of the response.
  std::string description;
  bool has_content = false;
  ParseProactiveSuggestionsMetadata(*loader_->ResponseInfo()->headers,
                                    &description, &has_content);

  // When the server indicates that there is no content we return a null set of
  // proactive suggestions.
  if (!has_content) {
    std::move(complete_callback_).Run(/*proactive_suggestions=*/nullptr);
    return;
  }

  // Otherwise we have a populated proactive suggestions response and can return
  // a fully constructed set of proactive suggestions.
  std::move(complete_callback_)
      .Run(base::MakeRefCounted<ash::ProactiveSuggestions>(
          /*description=*/description, /*html=*/std::move(*response_body)));
}
