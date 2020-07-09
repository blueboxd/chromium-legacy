// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"

#include "base/logging.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

using chrome::kWellKnownChangePasswordPath;
using chrome::kWellKnownNotExistingResourcePath;
using content::NavigationHandle;
using content::NavigationThrottle;
using content::WebContents;

// Used to scope the posted navigation task to the lifetime of |web_contents|.
class WebContentsLifetimeHelper
    : public content::WebContentsUserData<WebContentsLifetimeHelper> {
 public:
  explicit WebContentsLifetimeHelper(WebContents* web_contents)
      : web_contents_(web_contents) {}

  base::WeakPtr<WebContentsLifetimeHelper> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void NavigateTo(const content::OpenURLParams& url_params) {
    web_contents_->OpenURL(url_params);
  }

 private:
  friend class content::WebContentsUserData<WebContentsLifetimeHelper>;

  WebContents* const web_contents_;
  base::WeakPtrFactory<WebContentsLifetimeHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsLifetimeHelper)

bool IsWellKnownChangePasswordUrl(const GURL& url) {
  if (!url.is_valid() || !url.has_path())
    return false;
  base::StringPiece path = url.PathForRequestPiece();
  // remove trailing slash if there
  if (path.ends_with("/"))
    path = path.substr(0, path.size() - 1);
  return path == kWellKnownChangePasswordPath;
}

GURL CreateNonExistingResourceURL(const GURL& url) {
  GURL::Replacements replacement;
  replacement.SetPathStr(kWellKnownNotExistingResourcePath);
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
  url_loader_.reset();
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
WellKnownChangePasswordNavigationThrottle::WillProcessResponse() {
  change_password_response_code_ =
      navigation_handle()->GetResponseHeaders()->response_code();
  return BothRequestsFinished() ? ContinueProcessing()
                                : NavigationThrottle::DEFER;
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
  auto url_loader_factory = content::BrowserContext::GetDefaultStoragePartition(
                                handle->GetWebContents()->GetBrowserContext())
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
  if (!headers) {
    non_existing_resource_response_code_ = -1;
    return;
  }
  non_existing_resource_response_code_ = headers->response_code();
  if (BothRequestsFinished()) {
    ThrottleAction action = ContinueProcessing();
    if (action == NavigationThrottle::PROCEED) {
      Resume();
    }
  }
}

NavigationThrottle::ThrottleAction
WellKnownChangePasswordNavigationThrottle::ContinueProcessing() {
  DCHECK(BothRequestsFinished());
  if (SupportsChangePasswordUrl()) {
    return NavigationThrottle::PROCEED;
  } else {
    // TODO(crbug.com/1086141): Integrate Service that provides URL overrides
    Redirect(navigation_handle()->GetURL().GetOrigin());
    return NavigationThrottle::CANCEL;
  }
}

void WellKnownChangePasswordNavigationThrottle::Redirect(const GURL& url) {
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.url = url;
  params.transition = ui::PAGE_TRANSITION_CLIENT_REDIRECT;

  WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents)
    return;

  WebContentsLifetimeHelper::CreateForWebContents(web_contents);
  WebContentsLifetimeHelper* helper =
      WebContentsLifetimeHelper::FromWebContents(web_contents);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&WebContentsLifetimeHelper::NavigateTo,
                                helper->GetWeakPtr(), std::move(params)));
}

bool WellKnownChangePasswordNavigationThrottle::BothRequestsFinished() const {
  return non_existing_resource_response_code_ != 0 &&
         change_password_response_code_ != 0;
}

bool WellKnownChangePasswordNavigationThrottle::SupportsChangePasswordUrl()
    const {
  DCHECK(BothRequestsFinished());
  return 200 <= change_password_response_code_ &&
         change_password_response_code_ < 300 &&
         non_existing_resource_response_code_ == net::HTTP_NOT_FOUND;
}
