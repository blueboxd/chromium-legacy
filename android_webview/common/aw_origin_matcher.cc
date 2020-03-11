// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_origin_matcher.h"

#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/parse_number.h"
#include "net/base/scheme_host_port_matcher_rule.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace android_webview {

namespace {

// A rule to match all origins even the origin is opaque.
class MatchAllOriginsRule : public net::SchemeHostPortMatcherRule {
 public:
  MatchAllOriginsRule() = default;
  // Don't allow copy and assign.
  MatchAllOriginsRule(const MatchAllOriginsRule&) = delete;
  MatchAllOriginsRule& operator=(const MatchAllOriginsRule&) = delete;

  // net::SchemeHostPortMatcherRule implementation.
  net::SchemeHostPortMatcherResult Evaluate(const GURL& url) const override {
    return net::SchemeHostPortMatcherResult::kInclude;
  }

  std::string ToString() const override { return "*"; }
};

class SubdomainMatchingRule : public net ::SchemeHostPortMatcherRule {
 public:
  SubdomainMatchingRule(const std::string& scheme,
                        const std::string& optional_host,
                        int optional_port)
      : scheme_(base::ToLowerASCII(scheme)),
        optional_host_(base::ToLowerASCII(optional_host)),
        optional_port_(optional_port) {
    // We don't allow empty scheme.
    DCHECK(!scheme.empty());
  }
  // Don't allow copy and assign.
  SubdomainMatchingRule(const SubdomainMatchingRule&) = delete;
  SubdomainMatchingRule& operator=(const SubdomainMatchingRule&) = delete;

  // net::SchemeHostPortMatcherRule implementation.
  net::SchemeHostPortMatcherResult Evaluate(const GURL& url) const override {
    if (optional_port_ != -1 && url.EffectiveIntPort() != optional_port_) {
      // Didn't match port expectation.
      return net::SchemeHostPortMatcherResult::kNoMatch;
    }

    if (url.scheme() != scheme_) {
      // Didn't match scheme expectation.
      return net::SchemeHostPortMatcherResult::kNoMatch;
    }

    return base::MatchPattern(url.host(), optional_host_)
               ? net::SchemeHostPortMatcherResult::kInclude
               : net::SchemeHostPortMatcherResult::kNoMatch;
  }

  std::string ToString() const override {
    std::string str;
    base::StringAppendF(&str, "%s://%s", scheme_.c_str(),
                        optional_host_.c_str());
    if (optional_port_ != -1)
      base::StringAppendF(&str, ":%d", optional_port_);
    return str;
  }

 private:
  const std::string scheme_;
  // Empty string means no host provided.
  const std::string optional_host_;
  // -1 means no port provided.
  const int optional_port_;
};

inline bool HostWildcardSanityCheck(const std::string& host) {
  size_t wildcard_count = std::count(host.begin(), host.end(), '*');
  if (wildcard_count == 0)
    return true;

  // We only allow one wildcard.
  if (wildcard_count > 1)
    return false;

  // Start with "*." for subdomain matching.
  if (base::StartsWith(host, "*.", base::CompareCase::SENSITIVE))
    return true;

  return false;
}

inline int GetDefaultPortForSchemeIfNoPortInfo(const std::string& scheme,
                                               int port) {
  // The input has explicit port information, so don't modify it.
  if (port != -1)
    return port;

  // Hard code the port for http and https.
  if (scheme == url::kHttpScheme)
    return 80;
  if (scheme == url::kHttpsScheme)
    return 443;

  return port;
}

inline bool CanHaveHostPort(const std::string& scheme) {
  // We only allow http and https schemes to have host/port parts.
  return scheme == url::kHttpScheme || scheme == url::kHttpsScheme;
}

}  // namespace

AwOriginMatcher::AwOriginMatcher(const AwOriginMatcher& rhs) {
  *this = rhs;
}

AwOriginMatcher& AwOriginMatcher::operator=(const AwOriginMatcher& rhs) {
  rules_.clear();
  for (const auto& rule : rhs.Serialize()) {
    AddRuleFromString(rule);
  }
  return *this;
}

bool AwOriginMatcher::AddRuleFromString(const std::string& raw_untrimmed) {
  std::string raw;
  base::TrimWhitespaceASCII(raw_untrimmed, base::TRIM_ALL, &raw);

  if (raw == "*") {
    rules_.push_back(std::make_unique<MatchAllOriginsRule>());
    return true;
  }

  // Extract scheme-restriction.
  std::string::size_type scheme_pos = raw.find("://");
  if (scheme_pos == std::string::npos)
    return false;

  std::string scheme = raw.substr(0, scheme_pos);
  // Scheme is necessary for us and wildcard matching for scheme is not allowed.
  if (scheme.empty() || scheme.find('*') != std::string::npos)
    return false;

  std::string host_and_port = raw.substr(scheme_pos + 3);
  if (host_and_port.empty()) {
    // Doesn't allow "https://" and "http://" rules.
    if (CanHaveHostPort(scheme))
      return false;

    rules_.push_back(std::make_unique<SubdomainMatchingRule>(scheme, "", -1));
    return true;
  }

  // Now host_and_port is non-empty so scheme can't be other than https or http.
  if (!CanHaveHostPort(scheme))
    return false;

  // Now scheme is https or http and may have host and port.

  // URL like rule is invalid.
  if (host_and_port.find('/') != std::string::npos)
    return false;

  std::string host;
  int port;
  if (!net::ParseHostAndPort(host_and_port, &host, &port))
    return false;

  // Check if we have an <ip-address>[:port] input and try to canonicalize the
  // IP literal.
  net::IPAddress ip_address;
  if (ip_address.AssignFromIPLiteral(host)) {
    port = GetDefaultPortForSchemeIfNoPortInfo(scheme, port);
    host = ip_address.ToString();
    if (ip_address.IsIPv6()) {
      host = '[' + host + ']';
    }
    rules_.push_back(
        std::make_unique<SubdomainMatchingRule>(scheme, host, port));
    return true;
  }

  // Otherwise assume we have <hostname-pattern>[:port].
  if (!HostWildcardSanityCheck(host))
    return false;

  port = GetDefaultPortForSchemeIfNoPortInfo(scheme, port);
  rules_.push_back(std::make_unique<SubdomainMatchingRule>(scheme, host, port));
  return true;
}

bool AwOriginMatcher::Matches(const url::Origin& origin) const {
  GURL origin_url = origin.GetURL();
  // Since we only do kInclude vs kNoMatch, the order doesn't actually matter.
  for (auto it = rules_.rbegin(); it != rules_.rend(); ++it) {
    net::SchemeHostPortMatcherResult result = (*it)->Evaluate(origin_url);
    if (result == net::SchemeHostPortMatcherResult::kInclude)
      return true;
  }
  return false;
}

std::vector<std::string> AwOriginMatcher::Serialize() const {
  std::vector<std::string> result;
  result.reserve(rules_.size());
  for (const auto& rule : rules_) {
    result.push_back(rule->ToString());
  }
  return result;
}

}  // namespace android_webview
