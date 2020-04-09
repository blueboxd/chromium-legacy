// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_SITE_FOR_COOKIES_H_
#define NET_COOKIES_SITE_FOR_COOKIES_H_

#include <string>

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

// Represents which origins are to be considered first-party for a given
// context (e.g. frame). There may be none.
//
// The currently implemented policy is that two valid URLs would be considered
// the same party if either:
// 1) They both have non-empty and equal registrable domains or hostnames/IPs.
// 2) They both have empty hostnames and equal schemes.
// Invalid URLs are not first party to anything.
//
// TODO(crbug.com/1030938): For case 1 the schemes must be "https" & "wss",
// "http" & "ws", or they must match exactly.
class NET_EXPORT SiteForCookies {
 public:
  // Matches nothing.
  SiteForCookies();

  SiteForCookies(const SiteForCookies& other);
  SiteForCookies(SiteForCookies&& other);

  ~SiteForCookies();

  SiteForCookies& operator=(const SiteForCookies& other);
  SiteForCookies& operator=(SiteForCookies&& other);

  // Tries to construct an instance from (potentially untrusted) values of
  // scheme() and registrable_domain() that got received over an RPC.
  //
  // Returns whether successful or not. Doesn't touch |*out| if false is
  // returned.  This returning |true| does not mean that whoever sent the values
  // did not lie, merely that they are well-formed.
  static bool FromWire(const std::string& scheme,
                       const std::string& registrable_domain,
                       bool schemefully_same,
                       SiteForCookies* out);

  // If the origin is opaque, returns SiteForCookies that matches nothing.
  //
  // If it's not, returns one that matches URLs which are considered to be
  // same-party as URLs from |origin|.
  static SiteForCookies FromOrigin(const url::Origin& origin);

  // Equivalent to FromOrigin(url::Origin::Create(url)).
  static SiteForCookies FromUrl(const GURL& url);

  std::string ToDebugString() const;

  // Returns true if |url| should be considered first-party to the context
  // |this| represents.
  bool IsFirstParty(const GURL& url) const;

  // Returns true if |other.IsFirstParty()| is true for exactly the same URLs
  // as |this->IsFirstParty| (potentially none).
  bool IsEquivalent(const SiteForCookies& other) const;

  // Clears the schemefully_same_ flag if |other|'s scheme is cross-scheme to
  // |this|.
  // Two schemes are considered the same (not cross-scheme) if they exactly
  // match, they are both in ["https", "wss"], or they are both in ["http",
  // "ws"]. All other cases are cross-scheme.
  void MarkIfCrossScheme(const url::Origin& other);

  // Returns a URL that's first party to this SiteForCookies (an empty URL if
  // none) --- that is, it has the property that
  // site_for_cookies.IsEquivalent(
  //     SiteForCookies::FromUrl(site_for_cookies.RepresentativeUrl()));
  //
  // The convention used here (empty for nothing) is equivalent to that
  // used before SiteForCookies existed as a type; this method is mostly
  // meant to help incrementally migrate towards the type. New code probably
  // should not need this.
  GURL RepresentativeUrl() const;

  // Guaranteed to be lowercase.
  const std::string& scheme() const { return scheme_; }

  const std::string& registrable_domain() const { return registrable_domain_; }

  // Used for serialization/deserialization. This value is irrelevant if
  // IsNull() is true.
  bool schemefully_same() const { return schemefully_same_; }

  // Returns true if this SiteForCookies matches nothing.
  bool IsNull() const { return scheme_.empty(); }

 private:
  SiteForCookies(const std::string& scheme, const std::string& host);

  // These should be canonicalized appropriately by GURL/url::Origin.
  // An empty |scheme_| means that this matches nothing.
  std::string scheme_;

  // Represents which host or family of hosts this represents.
  // This is usually an eTLD+1 when one exists, but lacking that it may be
  // just the bare hostname or IP, or an empty string if this represents
  // something like file:///
  std::string registrable_domain_;

  // Used to indicate if the SiteForCookies would be the same if computed
  // schemefully. A schemeful computation means to take the |scheme_| as well as
  // the |registrable_domain_| into account when determining first-partyness.
  // See MarkIfCrossScheme() for more information on scheme comparison.
  //
  // True means to treat |this| as-is while false means that |this| should be
  // treated as if it matches nothing i.e. as if IsNull() returned true.
  //
  // This value is important in the case where the SiteForCookies is being used
  // to assess the first-partyness of a sub-frame in a document.
  //
  // For a SiteForCookies with !scheme_.empty() this value starts as true and
  // will only go false via MarkIfCrossScheme(), otherwise this value is
  // irrelevant.
  //
  // TODO(https://crbug.com/1030938): Actually use this for decisions in other
  // functions.
  bool schemefully_same_;
};

}  // namespace net

#endif  // NET_COOKIES_SITE_FOR_COOKIES_H_
