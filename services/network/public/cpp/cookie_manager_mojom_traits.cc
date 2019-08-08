// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_manager_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

network::mojom::CookiePriority
EnumTraits<network::mojom::CookiePriority, net::CookiePriority>::ToMojom(
    net::CookiePriority input) {
  switch (input) {
    case net::COOKIE_PRIORITY_LOW:
      return network::mojom::CookiePriority::LOW;
    case net::COOKIE_PRIORITY_MEDIUM:
      return network::mojom::CookiePriority::MEDIUM;
    case net::COOKIE_PRIORITY_HIGH:
      return network::mojom::CookiePriority::HIGH;
  }
  NOTREACHED();
  return static_cast<network::mojom::CookiePriority>(input);
}

bool EnumTraits<network::mojom::CookiePriority, net::CookiePriority>::FromMojom(
    network::mojom::CookiePriority input,
    net::CookiePriority* output) {
  switch (input) {
    case network::mojom::CookiePriority::LOW:
      *output = net::CookiePriority::COOKIE_PRIORITY_LOW;
      return true;
    case network::mojom::CookiePriority::MEDIUM:
      *output = net::CookiePriority::COOKIE_PRIORITY_MEDIUM;
      return true;
    case network::mojom::CookiePriority::HIGH:
      *output = net::CookiePriority::COOKIE_PRIORITY_HIGH;
      return true;
  }
  return false;
}

network::mojom::CookieSameSite
EnumTraits<network::mojom::CookieSameSite, net::CookieSameSite>::ToMojom(
    net::CookieSameSite input) {
  switch (input) {
    case net::CookieSameSite::UNSPECIFIED:
      return network::mojom::CookieSameSite::UNSPECIFIED;
    case net::CookieSameSite::NO_RESTRICTION:
      return network::mojom::CookieSameSite::NO_RESTRICTION;
    case net::CookieSameSite::LAX_MODE:
      return network::mojom::CookieSameSite::LAX_MODE;
    case net::CookieSameSite::STRICT_MODE:
      return network::mojom::CookieSameSite::STRICT_MODE;
    case net::CookieSameSite::EXTENDED_MODE:
      return network::mojom::CookieSameSite::EXTENDED_MODE;
    default:
      break;
  }
  NOTREACHED();
  return static_cast<network::mojom::CookieSameSite>(input);
}

bool EnumTraits<network::mojom::CookieSameSite, net::CookieSameSite>::FromMojom(
    network::mojom::CookieSameSite input,
    net::CookieSameSite* output) {
  switch (input) {
    case network::mojom::CookieSameSite::UNSPECIFIED:
      *output = net::CookieSameSite::UNSPECIFIED;
      return true;
    case network::mojom::CookieSameSite::NO_RESTRICTION:
      *output = net::CookieSameSite::NO_RESTRICTION;
      return true;
    case network::mojom::CookieSameSite::LAX_MODE:
      *output = net::CookieSameSite::LAX_MODE;
      return true;
    case network::mojom::CookieSameSite::STRICT_MODE:
      *output = net::CookieSameSite::STRICT_MODE;
      return true;
    case network::mojom::CookieSameSite::EXTENDED_MODE:
      *output = net::CookieSameSite::EXTENDED_MODE;
      return true;
    default:
      break;
  }
  return false;
}

network::mojom::CookieInclusionStatus
EnumTraits<network::mojom::CookieInclusionStatus,
           net::CanonicalCookie::CookieInclusionStatus>::
    ToMojom(net::CanonicalCookie::CookieInclusionStatus input) {
  switch (input) {
    case net::CanonicalCookie::CookieInclusionStatus::INCLUDE:
      return network::mojom::CookieInclusionStatus::INCLUDE;
    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_HTTP_ONLY:
      return network::mojom::CookieInclusionStatus::EXCLUDE_HTTP_ONLY;
    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SECURE_ONLY:
      return network::mojom::CookieInclusionStatus::EXCLUDE_SECURE_ONLY;
    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH:
      return network::mojom::CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH;
    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_NOT_ON_PATH:
      return network::mojom::CookieInclusionStatus::EXCLUDE_NOT_ON_PATH;
    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT:
      return network::mojom::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT;
    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX:
      return network::mojom::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX;
    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_EXTENDED:
      return network::mojom::CookieInclusionStatus::EXCLUDE_SAMESITE_EXTENDED;
    case net::CanonicalCookie::CookieInclusionStatus::
        EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX:
      return network::mojom::CookieInclusionStatus::
          EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX;
    case net::CanonicalCookie::CookieInclusionStatus::
        EXCLUDE_SAMESITE_NONE_INSECURE:
      return network::mojom::CookieInclusionStatus::
          EXCLUDE_SAMESITE_NONE_INSECURE;
    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES:
      return network::mojom::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES;

    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE:
      return network::mojom::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE;
    case net::CanonicalCookie::CookieInclusionStatus::
        EXCLUDE_NONCOOKIEABLE_SCHEME:
      return network::mojom::CookieInclusionStatus::
          EXCLUDE_NONCOOKIEABLE_SCHEME;
    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE:
      return network::mojom::CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE;
    case net::CanonicalCookie::CookieInclusionStatus::
        EXCLUDE_OVERWRITE_HTTP_ONLY:
      return network::mojom::CookieInclusionStatus::EXCLUDE_OVERWRITE_HTTP_ONLY;
    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN:
      return network::mojom::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN;
    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX:
      return network::mojom::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX;

    case net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR:
      return network::mojom::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR;
  }
  NOTREACHED();
  return network::mojom::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR;
}

bool EnumTraits<network::mojom::CookieInclusionStatus,
                net::CanonicalCookie::CookieInclusionStatus>::
    FromMojom(network::mojom::CookieInclusionStatus input,
              net::CanonicalCookie::CookieInclusionStatus* output) {
  switch (input) {
    case network::mojom::CookieInclusionStatus::INCLUDE:
      *output = net::CanonicalCookie::CookieInclusionStatus::INCLUDE;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_HTTP_ONLY:
      *output = net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_HTTP_ONLY;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_SECURE_ONLY:
      *output =
          net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SECURE_ONLY;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH:
      *output =
          net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_NOT_ON_PATH:
      *output =
          net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_NOT_ON_PATH;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT:
      *output =
          net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX:
      *output =
          net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_SAMESITE_EXTENDED:
      *output = net::CanonicalCookie::CookieInclusionStatus::
          EXCLUDE_SAMESITE_EXTENDED;
      return true;
    case network::mojom::CookieInclusionStatus::
        EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX:
      *output = net::CanonicalCookie::CookieInclusionStatus::
          EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE:
      *output = net::CanonicalCookie::CookieInclusionStatus::
          EXCLUDE_SAMESITE_NONE_INSECURE;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES:
      *output =
          net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE:
      *output =
          net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_NONCOOKIEABLE_SCHEME:
      *output = net::CanonicalCookie::CookieInclusionStatus::
          EXCLUDE_NONCOOKIEABLE_SCHEME;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE:
      *output =
          net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_OVERWRITE_HTTP_ONLY:
      *output = net::CanonicalCookie::CookieInclusionStatus::
          EXCLUDE_OVERWRITE_HTTP_ONLY;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN:
      *output =
          net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX:
      *output =
          net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX;
      return true;
    case network::mojom::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR:
      *output =
          net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR;
      return true;
  }
  NOTREACHED();
  return false;
}

network::mojom::CookieSameSiteContext
EnumTraits<network::mojom::CookieSameSiteContext,
           net::CookieOptions::SameSiteCookieContext>::
    ToMojom(net::CookieOptions::SameSiteCookieContext input) {
  switch (input) {
    case net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT:
      return network::mojom::CookieSameSiteContext::SAME_SITE_STRICT;
    case net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX:
      return network::mojom::CookieSameSiteContext::SAME_SITE_LAX;
    case net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE:
      return network::mojom::CookieSameSiteContext::SAME_SITE_LAX_METHOD_UNSAFE;
    case net::CookieOptions::SameSiteCookieContext::CROSS_SITE:
      return network::mojom::CookieSameSiteContext::CROSS_SITE;
  }
  NOTREACHED();
  return network::mojom::CookieSameSiteContext::CROSS_SITE;
}

bool EnumTraits<network::mojom::CookieSameSiteContext,
                net::CookieOptions::SameSiteCookieContext>::
    FromMojom(network::mojom::CookieSameSiteContext input,
              net::CookieOptions::SameSiteCookieContext* output) {
  switch (input) {
    case network::mojom::CookieSameSiteContext::SAME_SITE_STRICT:
      *output = net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT;
      return true;
    case network::mojom::CookieSameSiteContext::SAME_SITE_LAX:
      *output = net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX;
      return true;
    case network::mojom::CookieSameSiteContext::SAME_SITE_LAX_METHOD_UNSAFE:
      *output = net::CookieOptions::SameSiteCookieContext::
          SAME_SITE_LAX_METHOD_UNSAFE;
      return true;
    case network::mojom::CookieSameSiteContext::CROSS_SITE:
      *output = net::CookieOptions::SameSiteCookieContext::CROSS_SITE;
      return true;
  }
  return false;
}

bool StructTraits<network::mojom::CookieOptionsDataView, net::CookieOptions>::
    Read(network::mojom::CookieOptionsDataView mojo_options,
         net::CookieOptions* cookie_options) {
  if (mojo_options.exclude_httponly())
    cookie_options->set_exclude_httponly();
  else
    cookie_options->set_include_httponly();

  net::CookieOptions::SameSiteCookieContext same_site_cookie_context;
  if (!mojo_options.ReadSameSiteCookieContext(&same_site_cookie_context))
    return false;
  cookie_options->set_same_site_cookie_context(same_site_cookie_context);

  if (mojo_options.update_access_time())
    cookie_options->set_update_access_time();
  else
    cookie_options->set_do_not_update_access_time();

  if (mojo_options.return_excluded_cookies())
    cookie_options->set_return_excluded_cookies();
  else
    cookie_options->unset_return_excluded_cookies();

  return true;
}

bool StructTraits<
    network::mojom::CanonicalCookieDataView,
    net::CanonicalCookie>::Read(network::mojom::CanonicalCookieDataView cookie,
                                net::CanonicalCookie* out) {
  std::string name;
  if (!cookie.ReadName(&name))
    return false;

  std::string value;
  if (!cookie.ReadValue(&value))
    return false;

  std::string domain;
  if (!cookie.ReadDomain(&domain))
    return false;

  std::string path;
  if (!cookie.ReadPath(&path))
    return false;

  base::Time creation_time;
  base::Time expiry_time;
  base::Time last_access_time;
  if (!cookie.ReadCreation(&creation_time))
    return false;

  if (!cookie.ReadExpiry(&expiry_time))
    return false;

  if (!cookie.ReadLastAccess(&last_access_time))
    return false;

  net::CookieSameSite site_restrictions;
  if (!cookie.ReadSiteRestrictions(&site_restrictions))
    return false;

  net::CookiePriority priority;
  if (!cookie.ReadPriority(&priority))
    return false;

  *out = net::CanonicalCookie(name, value, domain, path, creation_time,
                              expiry_time, last_access_time, cookie.secure(),
                              cookie.httponly(), site_restrictions, priority);
  return out->IsCanonical();
}

bool StructTraits<
    network::mojom::CookieWithStatusDataView,
    net::CookieWithStatus>::Read(network::mojom::CookieWithStatusDataView c,
                                 net::CookieWithStatus* out) {
  net::CanonicalCookie cookie;
  net::CanonicalCookie::CookieInclusionStatus status;
  if (!c.ReadCookie(&cookie))
    return false;
  if (!c.ReadStatus(&status))
    return false;

  *out = {cookie, status};

  return true;
}

bool StructTraits<network::mojom::CookieAndLineWithStatusDataView,
                  net::CookieAndLineWithStatus>::
    Read(network::mojom::CookieAndLineWithStatusDataView c,
         net::CookieAndLineWithStatus* out) {
  base::Optional<net::CanonicalCookie> cookie;
  std::string cookie_string;
  net::CanonicalCookie::CookieInclusionStatus status;
  if (!c.ReadCookie(&cookie))
    return false;
  if (!c.ReadCookieString(&cookie_string))
    return false;
  if (!c.ReadStatus(&status))
    return false;

  *out = {cookie, cookie_string, status};

  return true;
}

}  // namespace mojo
