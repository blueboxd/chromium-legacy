// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

using content::NavigationHandle;
using content::NavigationThrottle;

// This path should return 404. This enables us to check whether
// we can trust the server's response code.
// https://wicg.github.io/change-password-url/response-code-reliability.html#iana
constexpr char kNotExistingResourcePath[] =
    ".well-known/"
    "resource-that-should-not-exist-whose-status-code-should-not-be-200";

bool IsWellKnownChangePasswordUrl(const GURL& url) {
  return url.is_valid() && url.has_path() &&
         (url.PathForRequest() == "/.well-known/change-password" ||
          url.PathForRequest() == "/.well-known/change-password/");
}

GURL CreateNonExistingResourceURL(const GURL& url) {
  GURL::Replacements replacement;
  replacement.SetPathStr(kNotExistingResourcePath);
  return url.GetOrigin().ReplaceComponents(replacement);
}

}  // namespace

// static
std::unique_ptr<WellKnownChangePasswordNavigationThrottle>
WellKnownChangePasswordNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* handle) {
  const GURL& url = handle->GetURL();
  // The order is important. We have to check if it as a well-known change
  // password url first. We should only check the feature flag when the feature
  // would be used. Otherwise the we would not see a difference between control
  // and experiment groups on the dashboards.
  if (IsWellKnownChangePasswordUrl(url) &&
      base::FeatureList::IsEnabled(
          password_manager::features::kWellKnownChangePassword)) {
    return base::WrapUnique(
        new WellKnownChangePasswordNavigationThrottle(handle));
  }
  return nullptr;
}

WellKnownChangePasswordNavigationThrottle::
    WellKnownChangePasswordNavigationThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

WellKnownChangePasswordNavigationThrottle::
    ~WellKnownChangePasswordNavigationThrottle() = default;

NavigationThrottle::ThrottleCheckResult
WellKnownChangePasswordNavigationThrottle::WillStartRequest() {
  FetchNonExistingResource(navigation_handle());
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
WellKnownChangePasswordNavigationThrottle::WillFailRequest() {
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
WellKnownChangePasswordNavigationThrottle::WillProcessResponse() {
  change_password_response_code_ =
      navigation_handle()->GetResponseHeaders()->response_code();
  // TODO(crbug.com/927473) handle processing
  return NavigationThrottle::PROCEED;
}

const char* WellKnownChangePasswordNavigationThrottle::GetNameForLogging() {
  return "WellKnownChangePasswordNavigationThrottle";
}

void WellKnownChangePasswordNavigationThrottle::FetchNonExistingResource(
    NavigationHandle* handle) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = CreateNonExistingResourceURL(handle->GetURL());
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "well_known_path_that_should_not_exist",
          R"(
        semantics {
          sender: "Password Manager"
          description:
            "Check whether the site supports .well-known 'special' URLs."
            "If the website does not support the spec we navigate to the "
            "fallback url. See also "
"https://wicg.github.io/change-password-url/response-code-reliability.html#iana"
          trigger:
            "When the user clicks 'Change password' on "
            "chrome://settings/passwords, or when they visit the "
            "[ORIGIN]/.well-known/change-password special URL, Chrome makes "
            "this additional request. Chrome Password manager shows a button "
            "with the link in the password checkup for compromised passwords "
            "view (chrome://settings/passwords/check) and in a dialog when the "
            "user signs in using compromised credentials."
          data:
            "The request body is empty. No user data is included."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled."
          policy_exception_justification: "Essential for navigation."
        })");
  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(
          Profile::FromBrowserContext(
              handle->GetWebContents()->GetBrowserContext()))
          ->GetURLLoaderFactoryForBrowserProcess();
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  // Binding the callback to |this| is safe, because the navigationthrottle
  // defers if the request is not received yet. Thereby the throttle still exist
  // when the response arrives.
  url_loader_->DownloadHeadersOnly(
      url_loader_factory.get(),
      base::BindOnce(&WellKnownChangePasswordNavigationThrottle::
                         FetchNonExistingResourceCallback,
                     base::Unretained(this)));
}

void WellKnownChangePasswordNavigationThrottle::
    FetchNonExistingResourceCallback(
        scoped_refptr<net::HttpResponseHeaders> headers) {
  non_existing_resource_response_code_ = headers->response_code();
  // TODO(crbug.com/927473) handle processing
}
