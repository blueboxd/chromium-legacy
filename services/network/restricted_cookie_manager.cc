// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_cookie_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"  // for FALLTHROUGH;
#include "base/debug/crash_logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "services/network/cookie_managers_shared.h"
#include "services/network/cookie_settings.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace network {

namespace {

net::CookieOptions MakeOptionsForSet(mojom::RestrictedCookieManagerRole role,
                                     const GURL& url,
                                     const GURL& site_for_cookies) {
  net::CookieOptions options;
  if (role == mojom::RestrictedCookieManagerRole::SCRIPT) {
    options.set_exclude_httponly();  // Default, but make it explicit here.
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForScriptSet(url,
                                                             site_for_cookies));
  } else {
    // mojom::RestrictedCookieManagerRole::NETWORK
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForSubresource(
            url, site_for_cookies));
  }
  return options;
}

net::CookieOptions MakeOptionsForGet(mojom::RestrictedCookieManagerRole role,
                                     const GURL& url,
                                     const GURL& site_for_cookies) {
  // TODO(https://crbug.com/925311): Wire initiator here.
  net::CookieOptions options;
  if (role == mojom::RestrictedCookieManagerRole::SCRIPT) {
    options.set_exclude_httponly();  // Default, but make it explicit here.
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForScriptGet(
            url, site_for_cookies, base::nullopt /*initiator*/));
  } else {
    // mojom::RestrictedCookieManagerRole::NETWORK
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForSubresource(
            url, site_for_cookies));
  }
  return options;
}

}  // namespace

using CookieInclusionStatus = net::CanonicalCookie::CookieInclusionStatus;

class RestrictedCookieManager::Listener : public base::LinkNode<Listener> {
 public:
  Listener(net::CookieStore* cookie_store,
           const RestrictedCookieManager* restricted_cookie_manager,
           const GURL& url,
           const GURL& site_for_cookies,
           net::CookieOptions options,
           mojom::CookieChangeListenerPtr mojo_listener)
      : restricted_cookie_manager_(restricted_cookie_manager),
        url_(url),
        site_for_cookies_(site_for_cookies),
        options_(options),
        mojo_listener_(std::move(mojo_listener)) {
    // TODO(pwnall): add a constructor w/options to net::CookieChangeDispatcher.
    cookie_store_subscription_ =
        cookie_store->GetChangeDispatcher().AddCallbackForUrl(
            url,
            base::BindRepeating(
                &Listener::OnCookieChange,
                // Safe because net::CookieChangeDispatcher guarantees that the
                // callback will stop being called immediately after we remove
                // the subscription, and the cookie store lives on the same
                // thread as we do.
                base::Unretained(this)));
  }

  ~Listener() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  mojom::CookieChangeListenerPtr& mojo_listener() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mojo_listener_;
  }

 private:
  // net::CookieChangeDispatcher callback.
  void OnCookieChange(const net::CanonicalCookie& cookie,
                      net::CookieChangeCause cause) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (cookie.IncludeForRequestURL(url_, options_) !=
        CookieInclusionStatus::INCLUDE)
      return;

    // When a user blocks a site's access to cookies, the existing cookies are
    // not deleted. This check prevents the site from observing their cookies
    // being deleted at a later time, which can happen due to eviction or due to
    // the user explicitly deleting all cookies.
    if (!restricted_cookie_manager_->cookie_settings()->IsCookieAccessAllowed(
            url_, site_for_cookies_))
      return;

    mojo_listener_->OnCookieChange(cookie, ToCookieChangeCause(cause));
  }

  // The CookieChangeDispatcher subscription used by this listener.
  std::unique_ptr<net::CookieChangeSubscription> cookie_store_subscription_;

  // Raw pointer usage is safe because RestrictedCookieManager owns this
  // instance and is guaranteed to outlive it.
  const RestrictedCookieManager* const restricted_cookie_manager_;

  // The URL whose cookies this listener is interested in.
  const GURL url_;

  // Site context in which we're used; used for permission-checking.
  const GURL site_for_cookies_;

  // CanonicalCookie::IncludeForRequestURL options for this listener's interest.
  const net::CookieOptions options_;

  mojom::CookieChangeListenerPtr mojo_listener_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(Listener);
};

RestrictedCookieManager::RestrictedCookieManager(
    const mojom::RestrictedCookieManagerRole role,
    net::CookieStore* cookie_store,
    const CookieSettings* cookie_settings,
    const url::Origin& origin,
    mojom::NetworkContextClient* network_context_client,
    bool is_service_worker,
    int32_t process_id,
    int32_t frame_id)
    : role_(role),
      cookie_store_(cookie_store),
      cookie_settings_(cookie_settings),
      origin_(origin),
      network_context_client_(network_context_client),
      is_service_worker_(is_service_worker),
      process_id_(process_id),
      frame_id_(frame_id) {
  DCHECK(cookie_store);
}

RestrictedCookieManager::~RestrictedCookieManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::LinkNode<Listener>* node = listeners_.head();
  while (node != listeners_.end()) {
    Listener* listener_reference = node->value();
    node = node->next();
    // The entire list is going away, no need to remove nodes from it.
    delete listener_reference;
  }
}

void RestrictedCookieManager::GetAllForUrl(
    const GURL& url,
    const GURL& site_for_cookies,
    mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ValidateAccessToCookiesAt(url)) {
    std::move(callback).Run({});
    return;
  }

  // TODO(morlovich): Try to validate site_for_cookies as well.

  net::CookieOptions net_options =
      MakeOptionsForGet(role_, url, site_for_cookies);
  // TODO(https://crbug.com/977040): remove set_return_excluded_cookies() once
  //                                 removing deprecation warnings.
  net_options.set_return_excluded_cookies();

  cookie_store_->GetCookieListWithOptionsAsync(
      url, net_options,
      base::BindOnce(&RestrictedCookieManager::CookieListToGetAllForUrlCallback,
                     weak_ptr_factory_.GetWeakPtr(), url, site_for_cookies,
                     net_options, std::move(options), std::move(callback)));
}

void RestrictedCookieManager::CookieListToGetAllForUrlCallback(
    const GURL& url,
    const GURL& site_for_cookies,
    const net::CookieOptions& net_options,
    mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback,
    const net::CookieList& cookie_list,
    const net::CookieStatusList& excluded_cookies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool blocked =
      !cookie_settings_->IsCookieAccessAllowed(url, site_for_cookies);

  std::vector<net::CanonicalCookie> result;
  std::vector<net::CookieWithStatus> result_with_status;

  // TODO(https://crbug.com/977040): Remove once samesite tightening up is
  // rolled out.
  for (const auto& cookie_and_status : excluded_cookies) {
    if (cookie_and_status.status ==
            CookieInclusionStatus::
                EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX ||
        cookie_and_status.status ==
            CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE) {
      result_with_status.push_back(
          {cookie_and_status.cookie, cookie_and_status.status});
    }
  }

  if (!blocked)
    result.reserve(cookie_list.size());
  mojom::CookieMatchType match_type = options->match_type;
  const std::string& match_name = options->name;
  for (size_t i = 0; i < cookie_list.size(); ++i) {
    const net::CanonicalCookie& cookie = cookie_list[i];
    const std::string& cookie_name = cookie.Name();

    if (match_type == mojom::CookieMatchType::EQUALS) {
      if (cookie_name != match_name)
        continue;
    } else if (match_type == mojom::CookieMatchType::STARTS_WITH) {
      if (!base::StartsWith(cookie_name, match_name,
                            base::CompareCase::SENSITIVE)) {
        continue;
      }
    } else {
      NOTREACHED();
    }

    if (blocked) {
      result_with_status.push_back(
          {cookie, CookieInclusionStatus::EXCLUDE_USER_PREFERENCES});
    } else {
      result.push_back(cookie);
      result_with_status.push_back({cookie, CookieInclusionStatus::INCLUDE});

      // Warn about upcoming deprecations.
      // TODO(https://crbug.com/977040): Remove once samesite tightening up is
      // rolled out.
      CookieInclusionStatus eventual_status =
          net::cookie_util::CookieWouldBeExcludedDueToSameSite(cookie,
                                                               net_options);
      if (eventual_status != CookieInclusionStatus::INCLUDE) {
        result_with_status.push_back({cookie, eventual_status});
      }
    }
  }

  if (network_context_client_) {
    network_context_client_->OnCookiesRead(is_service_worker_, process_id_,
                                           frame_id_, url, site_for_cookies,
                                           result_with_status);
  }

  if (blocked) {
    DCHECK(result.empty());
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(std::move(result));
}

void RestrictedCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const GURL& url,
    const GURL& site_for_cookies,
    SetCanonicalCookieCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ValidateAccessToCookiesAt(url)) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(morlovich): Try to validate site_for_cookies as well.
  bool blocked =
      !cookie_settings_->IsCookieAccessAllowed(url, site_for_cookies);

  if (blocked) {
    if (network_context_client_) {
      std::vector<net::CookieWithStatus> result_with_status;
      result_with_status.push_back(
          {cookie, CookieInclusionStatus::EXCLUDE_USER_PREFERENCES});
      network_context_client_->OnCookiesChanged(
          is_service_worker_, process_id_, frame_id_, url, site_for_cookies,
          result_with_status);
    }
    std::move(callback).Run(false);
    return;
  }

  // TODO(pwnall): Validate the CanonicalCookie fields.

  base::Time now = base::Time::NowFromSystemTime();
  auto sanitized_cookie = std::make_unique<net::CanonicalCookie>(
      cookie.Name(), cookie.Value(), cookie.Domain(), cookie.Path(), now,
      cookie.ExpiryDate(), now, cookie.IsSecure(), cookie.IsHttpOnly(),
      cookie.SameSite(), cookie.Priority());
  net::CanonicalCookie cookie_copy = *sanitized_cookie;

  net::CookieOptions options = MakeOptionsForSet(role_, url, site_for_cookies);
  cookie_store_->SetCanonicalCookieAsync(
      std::move(sanitized_cookie), origin_.scheme(), options,
      base::BindOnce(&RestrictedCookieManager::SetCanonicalCookieResult,
                     weak_ptr_factory_.GetWeakPtr(), url, site_for_cookies,
                     cookie_copy, options, std::move(callback)));
}

void RestrictedCookieManager::SetCanonicalCookieResult(
    const GURL& url,
    const GURL& site_for_cookies,
    const net::CanonicalCookie& cookie,
    const net::CookieOptions& net_options,
    SetCanonicalCookieCallback user_callback,
    net::CanonicalCookie::CookieInclusionStatus status) {
  std::vector<net::CookieWithStatus> notify;
  // TODO(https://crbug.com/977040): Only report pure INCLUDE once samesite
  // tightening up is rolled out.
  DCHECK_NE(status, CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);

  if (network_context_client_) {
    switch (status) {
      case CookieInclusionStatus::INCLUDE: {
        CookieInclusionStatus eventual_status =
            net::cookie_util::CookieWouldBeExcludedDueToSameSite(cookie,
                                                                 net_options);
        if (eventual_status != CookieInclusionStatus::INCLUDE) {
          notify.push_back({cookie, eventual_status});
        }
        FALLTHROUGH;
      }
      case CookieInclusionStatus::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX:
      case CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE:
        notify.push_back({cookie, status});
        network_context_client_->OnCookiesChanged(
            is_service_worker_, process_id_, frame_id_, url, site_for_cookies,
            std::move(notify));
        break;

      default:
        break;
    }
  }
  std::move(user_callback).Run(status == CookieInclusionStatus::INCLUDE);
}

void RestrictedCookieManager::AddChangeListener(
    const GURL& url,
    const GURL& site_for_cookies,
    mojom::CookieChangeListenerPtr mojo_listener,
    AddChangeListenerCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ValidateAccessToCookiesAt(url)) {
    std::move(callback).Run();
    return;
  }

  net::CookieOptions net_options =
      MakeOptionsForGet(role_, url, site_for_cookies);
  auto listener =
      std::make_unique<Listener>(cookie_store_, this, url, site_for_cookies,
                                 net_options, std::move(mojo_listener));

  listener->mojo_listener().set_connection_error_handler(
      base::BindOnce(&RestrictedCookieManager::RemoveChangeListener,
                     weak_ptr_factory_.GetWeakPtr(),
                     // Safe because this owns the listener, so the listener is
                     // guaranteed to be alive for as long as the weak pointer
                     // above resolves.
                     base::Unretained(listener.get())));

  // The linked list takes over the Listener ownership.
  listeners_.Append(listener.release());
  std::move(callback).Run();
}

void RestrictedCookieManager::SetCookieFromString(
    const GURL& url,
    const GURL& site_for_cookies,
    const std::string& cookie,
    SetCookieFromStringCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net::CookieOptions options = MakeOptionsForSet(role_, url, site_for_cookies);
  std::unique_ptr<net::CanonicalCookie> parsed_cookie =
      net::CanonicalCookie::Create(url, cookie, base::Time::Now(), options);
  if (!parsed_cookie) {
    std::move(callback).Run();
    return;
  }

  // Further checks (origin_, settings), as well as logging done by
  // SetCanonicalCookie()
  SetCanonicalCookie(
      *parsed_cookie, url, site_for_cookies,
      base::BindOnce([](SetCookieFromStringCallback user_callback,
                        bool success) { std::move(user_callback).Run(); },
                     std::move(callback)));
}

void RestrictedCookieManager::GetCookiesString(
    const GURL& url,
    const GURL& site_for_cookies,
    GetCookiesStringCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Checks done by GetAllForUrl.

  // Match everything.
  auto match_options = mojom::CookieManagerGetOptions::New();
  match_options->name = "";
  match_options->match_type = mojom::CookieMatchType::STARTS_WITH;
  GetAllForUrl(url, site_for_cookies, std::move(match_options),
               base::BindOnce(
                   [](GetCookiesStringCallback user_callback,
                      const std::vector<net::CanonicalCookie>& cookies) {
                     std::move(user_callback)
                         .Run(net::CanonicalCookie::BuildCookieLine(cookies));
                   },
                   std::move(callback)));
}

void RestrictedCookieManager::CookiesEnabledFor(
    const GURL& url,
    const GURL& site_for_cookies,
    CookiesEnabledForCallback callback) {
  if (!ValidateAccessToCookiesAt(url)) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(
      cookie_settings_->IsCookieAccessAllowed(url, site_for_cookies));
}

void RestrictedCookieManager::RemoveChangeListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listener->RemoveFromList();
  delete listener;
}

bool RestrictedCookieManager::ValidateAccessToCookiesAt(const GURL& url) {
  if (origin_.IsSameOriginWith(url::Origin::Create(url)))
    return true;

  // TODO(https://crbug.com/983090): Remove the crash keys once fixed.
  static base::debug::CrashKeyString* bound_origin =
      base::debug::AllocateCrashKeyString(
          "restricted_cookie_manager_bound_origin",
          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(bound_origin, origin_.GetDebugString());

  static base::debug::CrashKeyString* url_origin =
      base::debug::AllocateCrashKeyString(
          "restricted_cookie_manager_url_origin",
          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(url_origin,
                                 url::Origin::Create(url).GetDebugString());

  mojo::ReportBadMessage("Incorrect url origin");
  return false;
}

}  // namespace network
