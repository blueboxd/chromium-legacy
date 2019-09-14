// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "net/http/http_response_headers.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace network {

namespace {

// Parses a "Content-Security-Policy" header.
// Only extracts the frame-ancestors directive.
base::Optional<base::StringPiece> ParseHeaderValue(base::StringPiece header) {
  // For each token returned by strictly splitting serialized on the
  // U+003B SEMICOLON character (;):
  // 1. Strip leading and trailing ASCII whitespace from token.
  // 2. If token is an empty string, continue.
  for (const auto& directive : base::SplitStringPiece(
           header, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    // 3. Let directive name be the result of collecting a sequence of
    // code points from token which are not ASCII whitespace.
    // 4. Set directive name to be the result of running ASCII lowercase
    // on directive name.
    size_t pos = directive.find_first_of(base::kWhitespaceASCII);
    if (pos == std::string::npos)
      continue;
    base::StringPiece name = directive.substr(0, pos);

    // 5. If policy's directive set contains a directive whose name is
    // directive name, continue.
    // We don't need to handle this case, since we early return as soon as we
    // find the 'frame-ancestors' directive.

    // 6. Let directive value be the result of splitting token on ASCII
    // whitespace.
    base::StringPiece value = directive.substr(pos + 1);

    // 7. Let directive be a new directive whose name is directive name,
    // and value is directive value.
    // 8. Append directive to policy's directive set.
    if (base::EqualsCaseInsensitiveASCII(name, "frame-ancestors"))
      return value;
  }

  return base::nullopt;
}

// https://www.w3.org/TR/CSP3/#grammardef-scheme-part
bool ParseScheme(base::StringPiece scheme, mojom::CSPSource* csp_source) {
  if (scheme.empty())
    return false;

  if (!base::IsAsciiAlpha(scheme[0]))
    return false;

  auto is_scheme_character = [](auto c) {
    return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '+' ||
           c == '-' || c == '.';
  };

  if (!std::all_of(scheme.begin() + 1, scheme.end(), is_scheme_character))
    return false;

  csp_source->scheme = scheme.as_string();

  return true;
}

// https://www.w3.org/TR/CSP3/#grammardef-host-part
bool ParseHost(base::StringPiece host, mojom::CSPSource* csp_source) {
  if (host.size() == 0)
    return false;

  // * || *.
  if (host[0] == '*') {
    if (host.size() == 1) {
      csp_source->is_host_wildcard = true;
      return true;
    }

    if (host[1] != '.')
      return false;

    csp_source->is_host_wildcard = true;
    host = host.substr(2);
  }

  if (host.empty())
    return false;

  for (const base::StringPiece& piece : base::SplitStringPiece(
           host, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (piece.empty() || !std::all_of(piece.begin(), piece.end(), [](auto c) {
          return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '-';
        }))
      return false;
  }
  csp_source->host = host.as_string();

  return true;
}

// https://www.w3.org/TR/CSP3/#grammardef-port-part
bool ParsePort(base::StringPiece port, mojom::CSPSource* csp_source) {
  if (port.empty())
    return false;

  if (base::EqualsCaseInsensitiveASCII(port, "*")) {
    csp_source->is_port_wildcard = true;
    return true;
  }

  if (!std::all_of(port.begin(), port.end(),
                   base::IsAsciiDigit<base::StringPiece::value_type>)) {
    return false;
  }

  return base::StringToInt(port, &csp_source->port);
}

// https://www.w3.org/TR/CSP3/#grammardef-path-part
bool ParsePath(base::StringPiece path, mojom::CSPSource* csp_source) {
  DCHECK_NE(0U, path.size());
  if (path[0] != '/')
    return false;

  // TODO(lfg): Emit a warning to the user when a path containing # or ? is
  // seen.
  path = path.substr(0, path.find_first_of("#?"));

  url::RawCanonOutputT<base::char16> unescaped;
  url::DecodeURLEscapeSequences(path.data(), path.size(),
                                url::DecodeURLMode::kUTF8OrIsomorphic,
                                &unescaped);
  base::UTF16ToUTF8(unescaped.data(), unescaped.length(), &csp_source->path);

  return true;
}

// Parses an ancestor source expression.
// https://www.w3.org/TR/CSP3/#grammardef-ancestor-source
base::Optional<mojom::CSPSourcePtr> ParseAncestorSource(
    base::StringPiece source) {
  if (base::EqualsCaseInsensitiveASCII(source, "'none'"))
    return base::nullopt;

  mojom::CSPSource csp_source;

  if (base::EqualsCaseInsensitiveASCII(source, "*")) {
    csp_source.is_host_wildcard = true;
    return csp_source.Clone();
  }

  if (base::EqualsCaseInsensitiveASCII(source, "'self'")) {
    csp_source.allow_self = true;
    return csp_source.Clone();
  }

  size_t position = source.find_first_of(":/");
  if (position != std::string::npos && source[position] == ':') {
    // scheme:
    //       ^
    if (position + 1 == source.size()) {
      if (ParseScheme(source.substr(0, position), &csp_source))
        return csp_source.Clone();
      return base::nullopt;
    }

    if (source[position + 1] == '/') {
      // scheme://
      //       ^
      if (position + 2 >= source.size() || source[position + 2] != '/')
        return base::nullopt;
      if (!ParseScheme(source.substr(0, position), &csp_source))
        return base::nullopt;
      source = source.substr(position + 3);
      position = source.find_first_of(":/");
    }
  }

  // host
  //     ^
  if (!ParseHost(source.substr(0, position), &csp_source))
    return base::nullopt;

  // If there's nothing more to parse (no port or path specified), return.
  if (position == std::string::npos)
    return csp_source.Clone();

  source = source.substr(position);

  // :\d*
  // ^
  if (source[0] == ':') {
    size_t port_end = source.find_first_of("/");
    base::StringPiece port = source.substr(
        1, port_end == std::string::npos ? std::string::npos : port_end - 1);
    if (!ParsePort(port, &csp_source))
      return base::nullopt;
    if (port_end == std::string::npos)
      return csp_source.Clone();

    source = source.substr(port_end);
  }

  // /
  // ^
  if (!source.empty() && !ParsePath(source, &csp_source))
    return base::nullopt;

  return csp_source.Clone();
}

// Parse ancestor-source-list grammar.
// https://www.w3.org/TR/CSP3/#directive-frame-ancestors
base::Optional<mojom::CSPSourceListPtr> ParseFrameAncestorsDirective(
    base::StringPiece frame_ancestors_value) {
  base::StringPiece value = base::TrimString(
      frame_ancestors_value, base::kWhitespaceASCII, base::TRIM_ALL);

  std::vector<mojom::CSPSourcePtr> sources;

  if (base::EqualsCaseInsensitiveASCII(value, "'none'"))
    return mojom::CSPSourceList::New(std::move(sources));

  for (const auto& source : base::SplitStringPiece(
           value, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (auto csp_source = ParseAncestorSource(source))
      sources.push_back(std::move(*csp_source));
    else
      return base::nullopt;
  }

  return mojom::CSPSourceList::New(std::move(sources));
}

}  // namespace

ContentSecurityPolicy::ContentSecurityPolicy() = default;
ContentSecurityPolicy::~ContentSecurityPolicy() = default;

ContentSecurityPolicy::ContentSecurityPolicy(
    mojom::ContentSecurityPolicyPtr content_security_policy_ptr)
    : content_security_policy_ptr_(std::move(content_security_policy_ptr)) {}

ContentSecurityPolicy::ContentSecurityPolicy(const ContentSecurityPolicy& other)
    : content_security_policy_ptr_(other.content_security_policy_ptr_.Clone()) {
}

ContentSecurityPolicy::ContentSecurityPolicy(ContentSecurityPolicy&& other) =
    default;

ContentSecurityPolicy& ContentSecurityPolicy::operator=(
    const ContentSecurityPolicy& other) {
  content_security_policy_ptr_ = other.content_security_policy_ptr_.Clone();

  return *this;
}

ContentSecurityPolicy::operator mojom::ContentSecurityPolicyPtr() const {
  return content_security_policy_ptr_.Clone();
}

bool ContentSecurityPolicy::Parse(const net::HttpResponseHeaders& headers) {
  size_t iter = 0;
  std::string header_value;
  bool success = true;
  while (headers.EnumerateHeader(&iter, "content-security-policy",
                                 &header_value)) {
    if (!ParseFrameAncestors(header_value))
      success = false;
  }
  return success;
}

bool ContentSecurityPolicy::Parse(base::StringPiece header_value) {
  return ParseFrameAncestors(header_value);
}

bool ContentSecurityPolicy::ParseFrameAncestors(
    base::StringPiece header_value) {
  // Parse the CSP directives.
  // https://www.w3.org/TR/CSP3/#parse-serialized-policy
  if (auto frame_ancestors_value = ParseHeaderValue(header_value)) {
    // TODO(lfg): Emit a warning to the user when parsing an invalid
    // expression.
    if (auto parsed_frame_ancestors =
            ParseFrameAncestorsDirective(*frame_ancestors_value)) {
      content_security_policy_ptr_ = mojom::ContentSecurityPolicy::New();
      content_security_policy_ptr_->frame_ancestors =
          std::move(*parsed_frame_ancestors);
      return true;
    }
  }
  return false;
}

}  // namespace network
