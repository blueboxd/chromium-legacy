// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/content_security_policy.h"

#include <sstream>
#include <string>
#include "base/containers/flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/cpp/content_security_policy/csp_source.h"
#include "services/network/public/cpp/content_security_policy/csp_source_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace network {

using CSPDirectiveName = mojom::CSPDirectiveName;
using DirectivesMap = base::flat_map<base::StringPiece, base::StringPiece>;

namespace {

static CSPDirectiveName CSPFallback(CSPDirectiveName directive,
                                    CSPDirectiveName original_directive) {
  switch (directive) {
    case CSPDirectiveName::ConnectSrc:
    case CSPDirectiveName::FontSrc:
    case CSPDirectiveName::ImgSrc:
    case CSPDirectiveName::ManifestSrc:
    case CSPDirectiveName::MediaSrc:
    case CSPDirectiveName::PrefetchSrc:
    case CSPDirectiveName::ObjectSrc:
    case CSPDirectiveName::ScriptSrc:
    case CSPDirectiveName::StyleSrc:
      return CSPDirectiveName::DefaultSrc;

    case CSPDirectiveName::ScriptSrcAttr:
    case CSPDirectiveName::ScriptSrcElem:
      return CSPDirectiveName::ScriptSrc;

    case CSPDirectiveName::StyleSrcAttr:
    case CSPDirectiveName::StyleSrcElem:
      return CSPDirectiveName::StyleSrc;

    case CSPDirectiveName::FrameSrc:
    case CSPDirectiveName::WorkerSrc:
      return CSPDirectiveName::ChildSrc;

    // Because the fallback chain of child-src can be different if we are
    // checking a worker or a frame request, we need to know the original type
    // of the request to decide. These are the fallback chains for worker-src
    // and frame-src specifically.

    // worker-src > child-src > script-src > default-src
    // frame-src > child-src > default-src

    // Since there are some situations and tests that will operate on the
    // `child-src` directive directly (like for example the EE subsumption
    // algorithm), we consider the child-src > default-src fallback path as the
    // "default" and the worker-src fallback path as an exception.
    case CSPDirectiveName::ChildSrc:
      if (original_directive == CSPDirectiveName::WorkerSrc)
        return CSPDirectiveName::ScriptSrc;

      return CSPDirectiveName::DefaultSrc;

    case CSPDirectiveName::BaseURI:
    case CSPDirectiveName::DefaultSrc:
    case CSPDirectiveName::FormAction:
    case CSPDirectiveName::FrameAncestors:
    case CSPDirectiveName::NavigateTo:
    case CSPDirectiveName::ReportTo:
    case CSPDirectiveName::ReportURI:
    case CSPDirectiveName::Sandbox:
    case CSPDirectiveName::TreatAsPublicAddress:
    case CSPDirectiveName::UpgradeInsecureRequests:
      return CSPDirectiveName::Unknown;
    case CSPDirectiveName::Unknown:
      NOTREACHED();
      return CSPDirectiveName::Unknown;
  }
}

std::string ElideURLForReportViolation(const GURL& url) {
  // TODO(arthursonzogni): the url length should be limited to 1024 char. Find
  // a function that will not break the utf8 encoding while eliding the string.
  return url.spec();
}

// Return the error message specific to one CSP |directive|.
// $1: Blocked URL.
// $2: Blocking policy.
const char* ErrorMessage(CSPDirectiveName directive) {
  switch (directive) {
    case CSPDirectiveName::FormAction:
      return "Refused to send form data to '$1' because it violates the "
             "following Content Security Policy directive: \"$2\".";
    case CSPDirectiveName::FrameAncestors:
      return "Refused to frame '$1' because an ancestor violates the following "
             "Content Security Policy directive: \"$2\".";
    case CSPDirectiveName::FrameSrc:
      return "Refused to frame '$1' because it violates the "
             "following Content Security Policy directive: \"$2\".";
    case CSPDirectiveName::NavigateTo:
      return "Refused to navigate to '$1' because it violates the "
             "following Content Security Policy directive: \"$2\".";

    case CSPDirectiveName::BaseURI:
    case CSPDirectiveName::ChildSrc:
    case CSPDirectiveName::ConnectSrc:
    case CSPDirectiveName::DefaultSrc:
    case CSPDirectiveName::FontSrc:
    case CSPDirectiveName::ImgSrc:
    case CSPDirectiveName::ManifestSrc:
    case CSPDirectiveName::MediaSrc:
    case CSPDirectiveName::ObjectSrc:
    case CSPDirectiveName::PrefetchSrc:
    case CSPDirectiveName::ReportTo:
    case CSPDirectiveName::ReportURI:
    case CSPDirectiveName::Sandbox:
    case CSPDirectiveName::ScriptSrc:
    case CSPDirectiveName::ScriptSrcAttr:
    case CSPDirectiveName::ScriptSrcElem:
    case CSPDirectiveName::StyleSrc:
    case CSPDirectiveName::StyleSrcAttr:
    case CSPDirectiveName::StyleSrcElem:
    case CSPDirectiveName::TreatAsPublicAddress:
    case CSPDirectiveName::UpgradeInsecureRequests:
    case CSPDirectiveName::WorkerSrc:
    case CSPDirectiveName::Unknown:
      NOTREACHED();
      return nullptr;
  };
}

void ReportViolation(CSPContext* context,
                     const mojom::ContentSecurityPolicyPtr& policy,
                     const CSPDirectiveName effective_directive_name,
                     const CSPDirectiveName directive_name,
                     const GURL& url,
                     bool has_followed_redirect,
                     const mojom::SourceLocationPtr& source_location) {
  // For security reasons, some urls must not be disclosed. This includes the
  // blocked url and the source location of the error. Care must be taken to
  // ensure that these are not transmitted between different cross-origin
  // renderers.
  GURL blocked_url = (directive_name == CSPDirectiveName::FrameAncestors)
                         ? GURL(ToString(context->self_source()))
                         : url;
  auto safe_source_location =
      source_location ? source_location->Clone() : mojom::SourceLocation::New();
  context->SanitizeDataForUseInCspViolation(has_followed_redirect,
                                            directive_name, &blocked_url,
                                            safe_source_location.get());

  std::stringstream message;

  if (policy->header->type == mojom::ContentSecurityPolicyType::kReport)
    message << "[Report Only] ";

  message << base::ReplaceStringPlaceholders(
      ErrorMessage(directive_name),
      {ElideURLForReportViolation(blocked_url),
       ToString(effective_directive_name) + " " +
           ToString(policy->directives[effective_directive_name])},
      nullptr);

  if (effective_directive_name != directive_name) {
    message << " Note that '" << ToString(directive_name)
            << "' was not explicitly set, so '"
            << ToString(effective_directive_name) << "' is used as a fallback.";
  }

  message << "\n";

  context->ReportContentSecurityPolicyViolation(mojom::CSPViolation::New(
      ToString(effective_directive_name), ToString(directive_name),
      message.str(), blocked_url, policy->report_endpoints,
      policy->use_reporting_api, policy->header->header_value,
      policy->header->type, has_followed_redirect,
      std::move(safe_source_location)));
}

const GURL ExtractInnerURL(const GURL& url) {
  if (const GURL* inner_url = url.inner_url())
    return *inner_url;
  else
    // TODO(arthursonzogni): revisit this once GURL::inner_url support blob-URL.
    return GURL(url.path());
}

bool ShouldBypassContentSecurityPolicy(CSPContext* context, const GURL& url) {
  if (url.SchemeIsFileSystem() || url.SchemeIsBlob()) {
    return context->SchemeShouldBypassCSP(ExtractInnerURL(url).scheme());
  } else {
    return context->SchemeShouldBypassCSP(url.scheme());
  }
}

// Parses a "Content-Security-Policy" header.
// Returns a map to the directives found.
DirectivesMap ParseHeaderValue(base::StringPiece header) {
  DirectivesMap result;

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
    base::StringPiece name = directive.substr(0, pos);

    // 5. If policy's directive set contains a directive whose name is
    // directive name, continue.
    if (result.find(name) != result.end())
      continue;

    // 6. Let directive value be the result of splitting token on ASCII
    // whitespace.
    base::StringPiece value;
    if (pos != std::string::npos)
      value = directive.substr(pos + 1);

    // 7. Let directive be a new directive whose name is directive name,
    // and value is directive value.
    // 8. Append directive to policy's directive set.
    result.insert({name, value});
  }

  return result;
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

// Parses a CSP source expression.
// https://w3c.github.io/webappsec-csp/#source-lists
//
// Return false on errors.
bool ParseSource(base::StringPiece expression, mojom::CSPSource* csp_source) {
  // TODO(arthursonzogni): Blink reports an invalid source expression when
  // 'none' is parsed here.
  if (base::EqualsCaseInsensitiveASCII(expression, "'none'"))
    return false;

  size_t position = expression.find_first_of(":/");
  if (position != std::string::npos && expression[position] == ':') {
    // scheme:
    //       ^
    if (position + 1 == expression.size())
      return ParseScheme(expression.substr(0, position), csp_source);

    if (expression[position + 1] == '/') {
      // scheme://
      //       ^
      if (position + 2 >= expression.size() || expression[position + 2] != '/')
        return false;
      if (!ParseScheme(expression.substr(0, position), csp_source))
        return false;
      expression = expression.substr(position + 3);
      position = expression.find_first_of(":/");
    }
  }

  // host
  //     ^
  if (!ParseHost(expression.substr(0, position), csp_source))
    return false;

  // If there's nothing more to parse (no port or path specified), return.
  if (position == std::string::npos)
    return true;

  expression = expression.substr(position);

  // :\d*
  // ^
  if (expression[0] == ':') {
    size_t port_end = expression.find_first_of("/");
    base::StringPiece port = expression.substr(
        1, port_end == std::string::npos ? std::string::npos : port_end - 1);
    if (!ParsePort(port, csp_source))
      return false;
    if (port_end == std::string::npos)
      return true;

    expression = expression.substr(port_end);
  }

  // /
  // ^
  return expression.empty() || ParsePath(expression, csp_source);
}

bool IsBase64Char(char c) {
  return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '+' ||
         c == '-' || c == '_' || c == '/';
}

int EatChar(const char** it, const char* end, bool (*predicate)(char)) {
  int count = 0;
  while (*it != end) {
    if (!predicate(**it))
      break;
    ++count;
    ++(*it);
  }
  return count;
}

// Checks whether |expression| is a valid base64-encoded string.
// Cf. https://w3c.github.io/webappsec-csp/#framework-directive-source-list.
bool IsBase64(base::StringPiece expression) {
  if (expression.empty())
    return false;

  auto* it = expression.begin();
  auto* end = expression.end();

  int count_1 = EatChar(&it, end, IsBase64Char);
  int count_2 = EatChar(&it, end, [](char c) -> bool { return c == '='; });

  // At least one non '=' char at the beginning, at most two '=' at the end.
  return count_1 >= 1 && count_2 <= 2 && it == end;
}

// Parse a nonce-source, return false on error.
bool ParseNonce(base::StringPiece expression, std::string* nonce) {
  if (!base::StartsWith(expression, "'nonce-",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }

  base::StringPiece subexpression =
      expression.substr(7, expression.length() - 8);

  if (!IsBase64(subexpression))
    return false;

  if (expression[expression.length() - 1] != '\'') {
    return false;
  }

  *nonce = subexpression.as_string();
  return true;
}

struct SupportedPrefixesStruct {
  const char* prefix;
  int prefix_length;
  mojom::CSPHashAlgorithm type;
};

// Parse a hash-source, return false on error.
bool ParseHash(base::StringPiece expression, mojom::CSPHashSource* hash) {
  static const SupportedPrefixesStruct SupportedPrefixes[] = {
      {"'sha256-", 8, mojom::CSPHashAlgorithm::SHA256},
      {"'sha384-", 8, mojom::CSPHashAlgorithm::SHA384},
      {"'sha512-", 8, mojom::CSPHashAlgorithm::SHA512},
      {"'sha-256-", 9, mojom::CSPHashAlgorithm::SHA256},
      {"'sha-384-", 9, mojom::CSPHashAlgorithm::SHA384},
      {"'sha-512-", 9, mojom::CSPHashAlgorithm::SHA512}};

  for (auto item : SupportedPrefixes) {
    if (base::StartsWith(expression, item.prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      base::StringPiece subexpression = expression.substr(
          item.prefix_length, expression.length() - item.prefix_length - 1);
      if (!IsBase64(subexpression))
        return false;

      if (expression[expression.length() - 1] != '\'')
        return false;

      hash->algorithm = item.type;
      hash->value = subexpression.as_string();
      return true;
    }
  }

  return false;
}

// Parse source-list grammar.
// https://www.w3.org/TR/CSP3/#grammardef-serialized-source-list
mojom::CSPSourceListPtr ParseSourceList(CSPDirectiveName directive_name,
                                        base::StringPiece directive_value) {
  base::StringPiece value =
      base::TrimString(directive_value, base::kWhitespaceASCII, base::TRIM_ALL);

  auto directive = mojom::CSPSourceList::New();

  if (base::EqualsCaseInsensitiveASCII(value, "'none'"))
    return directive;

  for (const auto& expression : base::SplitStringPiece(
           value, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    if (base::EqualsCaseInsensitiveASCII(expression, "'self'")) {
      directive->allow_self = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "*")) {
      directive->allow_star = true;
      continue;
    }

    auto csp_source = mojom::CSPSource::New();
    if (ParseSource(expression, csp_source.get())) {
      directive->sources.push_back(std::move(csp_source));
      continue;
    }

    if (directive_name == CSPDirectiveName::FrameAncestors) {
      // The frame-ancestors directive does not support anything else
      // https://w3c.github.io/webappsec-csp/#directive-frame-ancestors
      // TODO(antoniosartori): This is a parsing error, so we should emit a
      // warning.
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'unsafe-inline'")) {
      directive->allow_inline = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'unsafe-eval'")) {
      directive->allow_eval = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'wasm-eval'")) {
      directive->allow_wasm_eval = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression,
                                         "'unsafe-allow-redirects'") &&
        directive_name == CSPDirectiveName::NavigateTo) {
      directive->allow_response_redirects = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'strict-dynamic'")) {
      directive->allow_dynamic = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'unsafe-hashes'")) {
      directive->allow_unsafe_hashes = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'report-sample'")) {
      directive->report_sample = true;
      continue;
    }

    std::string nonce;
    if (ParseNonce(expression, &nonce)) {
      directive->nonces.push_back(std::move(nonce));
      continue;
    }

    auto hash = mojom::CSPHashSource::New();
    if (ParseHash(expression, hash.get())) {
      directive->hashes.push_back(std::move(hash));
      continue;
    }

    // Parsing error.
    // Ignore this source-expression.
    // TODO(lfg): Emit a warning to the user when parsing an invalid
    // expression.
  }

  return directive;
}

// Parses a reporting directive.
// https://w3c.github.io/webappsec-csp/#directives-reporting
// TODO(lfg): The report-to should be treated as a single token according to the
// spec, but this implementation accepts multiple endpoints
// https://crbug.com/916265.
void ParseReportDirective(const GURL& request_url,
                          base::StringPiece value,
                          bool using_reporting_api,
                          std::vector<std::string>* report_endpoints) {
  for (const auto& uri : base::SplitStringPiece(value, base::kWhitespaceASCII,
                                                base::TRIM_WHITESPACE,
                                                base::SPLIT_WANT_NONEMPTY)) {
    // There are two types of reporting directive:
    //
    // - "report-uri (uri)+"
    //   |uri| must be resolved relatively to the requested URL.
    //
    // - "report-to (endpoint)+"
    //   |endpoint| is an arbitrary string. It refers to an endpoint declared in
    //   the "Report-To" header. See https://w3c.github.io/reporting
    if (using_reporting_api) {
      report_endpoints->push_back(uri.as_string());

      // 'report-to' only allows for a single token. The following ones are
      // ignored. A console error warning is displayed from blink's CSP parser.
      break;
    } else {
      GURL url = request_url.Resolve(uri);

      // TODO(lfg): Emit a warning when parsing an invalid reporting URL.
      if (!url.is_valid())
        continue;
      report_endpoints->push_back(url.spec());
    }
  }
}

// Parses a directive of a Content-Security-Policy header that adheres to the
// source list grammar.
void ParseSourceListTypeDirective(const mojom::ContentSecurityPolicyPtr& policy,
                                  CSPDirectiveName directive_name,
                                  base::StringPiece value) {
  // A directive with this name has already been parsed. Skip further
  // directives per
  // https://www.w3.org/TR/CSP3/#parse-serialized-policy.
  // TODO(arthursonzogni, lfg): Should a warning be fired to the user here?
  if (policy->directives.count(directive_name))
    return;

  auto source_list = ParseSourceList(directive_name, value);

  // TODO(lfg): Emit a warning to the user when parsing an invalid
  // expression.
  if (!source_list)
    return;

  policy->directives[directive_name] = std::move(source_list);
}

// Parses the report-uri directive of a Content-Security-Policy header.
void ParseReportEndpoint(const mojom::ContentSecurityPolicyPtr& policy,
                         const GURL& base_url,
                         base::StringPiece header_value,
                         bool using_reporting_api) {
  // A report-uri directive has already been parsed. Skip further directives per
  // https://www.w3.org/TR/CSP3/#parse-serialized-policy.
  if (!policy->report_endpoints.empty())
    return;

  ParseReportDirective(base_url, header_value, using_reporting_api,
                       &(policy->report_endpoints));
}

void AddContentSecurityPolicyFromHeader(base::StringPiece header,
                                        mojom::ContentSecurityPolicyType type,
                                        const GURL& base_url,
                                        mojom::ContentSecurityPolicyPtr& out) {
  DirectivesMap directives = ParseHeaderValue(header);
  out->header = mojom::ContentSecurityPolicyHeader::New(
      header.as_string(), type, mojom::ContentSecurityPolicySource::kHTTP);

  for (auto directive : directives) {
    CSPDirectiveName directive_name =
        ToCSPDirectiveName(directive.first.as_string());
    switch (directive_name) {
      case CSPDirectiveName::BaseURI:
      case CSPDirectiveName::ChildSrc:
      case CSPDirectiveName::ConnectSrc:
      case CSPDirectiveName::DefaultSrc:
      case CSPDirectiveName::FontSrc:
      case CSPDirectiveName::FormAction:
      case CSPDirectiveName::FrameAncestors:
      case CSPDirectiveName::FrameSrc:
      case CSPDirectiveName::ImgSrc:
      case CSPDirectiveName::ManifestSrc:
      case CSPDirectiveName::MediaSrc:
      case CSPDirectiveName::NavigateTo:
      case CSPDirectiveName::ObjectSrc:
      case CSPDirectiveName::PrefetchSrc:
      case CSPDirectiveName::ScriptSrc:
      case CSPDirectiveName::ScriptSrcAttr:
      case CSPDirectiveName::ScriptSrcElem:
      case CSPDirectiveName::StyleSrc:
      case CSPDirectiveName::StyleSrcAttr:
      case CSPDirectiveName::StyleSrcElem:
      case CSPDirectiveName::WorkerSrc:
        ParseSourceListTypeDirective(out, directive_name, directive.second);
        break;
      case CSPDirectiveName::Sandbox:
        // Note: |ParseSandboxPolicy(...).error_message| is ignored here.
        // Blink's CSP parser is already in charge of displaying it.
        out->sandbox = ~ParseWebSandboxPolicy(directive.second,
                                              mojom::WebSandboxFlags::kNone)
                            .flags;
        break;
      case CSPDirectiveName::UpgradeInsecureRequests:
        out->upgrade_insecure_requests = true;
        break;
      case CSPDirectiveName::TreatAsPublicAddress:
        out->treat_as_public_address = true;
        break;
      case CSPDirectiveName::ReportTo:
        out->use_reporting_api = true;
        out->report_endpoints.clear();
        ParseReportEndpoint(out, base_url, directive.second,
                            out->use_reporting_api);
        break;
      case CSPDirectiveName::ReportURI:
        if (!out->use_reporting_api)
          ParseReportEndpoint(out, base_url, directive.second,
                              out->use_reporting_api);
        break;
      case CSPDirectiveName::Unknown:
        break;
    }
  }
}

}  // namespace

void AddContentSecurityPolicyFromHeaders(
    const net::HttpResponseHeaders& headers,
    const GURL& base_url,
    std::vector<mojom::ContentSecurityPolicyPtr>* out) {
  size_t iter = 0;
  std::string header_value;
  while (headers.EnumerateHeader(&iter, "content-security-policy",
                                 &header_value)) {
    AddContentSecurityPolicyFromHeaders(
        header_value, mojom::ContentSecurityPolicyType::kEnforce, base_url,
        out);
  }
  iter = 0;
  while (headers.EnumerateHeader(&iter, "content-security-policy-report-only",
                                 &header_value)) {
    AddContentSecurityPolicyFromHeaders(
        header_value, mojom::ContentSecurityPolicyType::kReport, base_url, out);
  }
}

void AddContentSecurityPolicyFromHeaders(
    base::StringPiece header_value,
    network::mojom::ContentSecurityPolicyType type,
    const GURL& base_url,
    std::vector<mojom::ContentSecurityPolicyPtr>* out) {
  // RFC7230, section 3.2.2 specifies that headers appearing multiple times can
  // be combined with a comma. Walk the header string, and parse each comma
  // separated chunk as a separate header.
  for (const auto& header :
       base::SplitStringPiece(header_value, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    auto policy = mojom::ContentSecurityPolicy::New();
    AddContentSecurityPolicyFromHeader(header, type, base_url, policy);

    out->push_back(std::move(policy));
  }
}

mojom::AllowCSPFromHeaderValuePtr ParseAllowCSPFromHeader(
    const net::HttpResponseHeaders& headers) {
  if (!base::FeatureList::IsEnabled(features::kOutOfBlinkCSPEE))
    return nullptr;

  std::string allow_csp_from;
  if (!headers.GetNormalizedHeader("Allow-CSP-From", &allow_csp_from))
    return nullptr;

  base::StringPiece trimmed =
      base::TrimWhitespaceASCII(allow_csp_from, base::TRIM_ALL);

  if (trimmed == "*")
    return mojom::AllowCSPFromHeaderValue::NewAllowStar(true);

  GURL parsed_url = GURL(trimmed);
  if (!parsed_url.is_valid()) {
    return mojom::AllowCSPFromHeaderValue::NewErrorMessage(
        "The 'Allow-CSP-From' header contains neither '*' nor a valid origin.");
  }
  return mojom::AllowCSPFromHeaderValue::NewOrigin(
      url::Origin::Create(parsed_url));
}

bool CheckContentSecurityPolicy(const mojom::ContentSecurityPolicyPtr& policy,
                                CSPDirectiveName directive_name,
                                const GURL& url,
                                bool has_followed_redirect,
                                bool is_response_check,
                                CSPContext* context,
                                const mojom::SourceLocationPtr& source_location,
                                bool is_form_submission) {
  if (ShouldBypassContentSecurityPolicy(context, url))
    return true;

  // 'navigate-to' has no effect when doing a form submission and a
  // 'form-action' directive is present.
  if (is_form_submission && directive_name == CSPDirectiveName::NavigateTo &&
      policy->directives.count(CSPDirectiveName::FormAction)) {
    return true;
  }

  for (CSPDirectiveName effective_directive_name = directive_name;
       effective_directive_name != CSPDirectiveName::Unknown;
       effective_directive_name =
           CSPFallback(effective_directive_name, directive_name)) {
    const auto& directive = policy->directives.find(effective_directive_name);
    if (directive == policy->directives.end())
      continue;

    const auto& source_list = directive->second;
    bool allowed = CheckCSPSourceList(source_list, url, context,
                                      has_followed_redirect, is_response_check);

    if (!allowed) {
      ReportViolation(context, policy, effective_directive_name, directive_name,
                      url, has_followed_redirect, source_location);
    }

    return allowed ||
           policy->header->type == mojom::ContentSecurityPolicyType::kReport;
  }
  return true;
}

bool ShouldUpgradeInsecureRequest(
    const std::vector<mojom::ContentSecurityPolicyPtr>& policies) {
  for (const auto& policy : policies) {
    if (policy->upgrade_insecure_requests)
      return true;
  }

  return false;
}

bool ShouldTreatAsPublicAddress(
    const std::vector<mojom::ContentSecurityPolicyPtr>& policies) {
  for (const auto& policy : policies) {
    if (policy->treat_as_public_address)
      return true;
  }

  return false;
}

void UpgradeInsecureRequest(GURL* url) {
  // Only HTTP URL can be upgraded to HTTPS.
  if (!url->SchemeIs(url::kHttpScheme))
    return;

  // Some URL like http://127.0.0.0.1 are considered potentially trustworthy and
  // aren't upgraded, even if the protocol used is HTTP.
  if (IsUrlPotentiallyTrustworthy(*url))
    return;

  // Updating the URL's scheme also implicitly updates the URL's port from
  // 80 to 443 if needed.
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpsScheme);
  *url = url->ReplaceComponents(replacements);
}

CSPDirectiveName ToCSPDirectiveName(const std::string& name) {
  if (name == "base-uri")
    return CSPDirectiveName::BaseURI;
  if (name == "child-src")
    return CSPDirectiveName::ChildSrc;
  if (name == "connect-src")
    return CSPDirectiveName::ConnectSrc;
  if (name == "default-src")
    return CSPDirectiveName::DefaultSrc;
  if (name == "frame-ancestors")
    return CSPDirectiveName::FrameAncestors;
  if (name == "frame-src")
    return CSPDirectiveName::FrameSrc;
  if (name == "font-src")
    return CSPDirectiveName::FontSrc;
  if (name == "form-action")
    return CSPDirectiveName::FormAction;
  if (name == "img-src")
    return CSPDirectiveName::ImgSrc;
  if (name == "manifest-src")
    return CSPDirectiveName::ManifestSrc;
  if (name == "media-src")
    return CSPDirectiveName::MediaSrc;
  if (name == "object-src")
    return CSPDirectiveName::ObjectSrc;
  if (name == "prefetch-src")
    return CSPDirectiveName::PrefetchSrc;
  if (name == "report-uri")
    return CSPDirectiveName::ReportURI;
  if (name == "sandbox")
    return CSPDirectiveName::Sandbox;
  if (name == "script-src")
    return CSPDirectiveName::ScriptSrc;
  if (name == "script-src-attr")
    return CSPDirectiveName::ScriptSrcAttr;
  if (name == "script-src-elem")
    return CSPDirectiveName::ScriptSrcElem;
  if (name == "style-src")
    return CSPDirectiveName::StyleSrc;
  if (name == "style-src-attr")
    return CSPDirectiveName::StyleSrcAttr;
  if (name == "style-src-elem")
    return CSPDirectiveName::StyleSrcElem;
  if (name == "treat-as-public-address")
    return CSPDirectiveName::TreatAsPublicAddress;
  if (name == "upgrade-insecure-requests")
    return CSPDirectiveName::UpgradeInsecureRequests;
  if (name == "worker-src")
    return CSPDirectiveName::WorkerSrc;
  if (name == "report-to")
    return CSPDirectiveName::ReportTo;
  if (name == "navigate-to")
    return CSPDirectiveName::NavigateTo;

  return CSPDirectiveName::Unknown;
}

std::string ToString(CSPDirectiveName name) {
  switch (name) {
    case CSPDirectiveName::BaseURI:
      return "base-uri";
    case CSPDirectiveName::ChildSrc:
      return "child-src";
    case CSPDirectiveName::ConnectSrc:
      return "connect-src";
    case CSPDirectiveName::DefaultSrc:
      return "default-src";
    case CSPDirectiveName::FrameAncestors:
      return "frame-ancestors";
    case CSPDirectiveName::FrameSrc:
      return "frame-src";
    case CSPDirectiveName::FontSrc:
      return "font-src";
    case CSPDirectiveName::FormAction:
      return "form-action";
    case CSPDirectiveName::ImgSrc:
      return "img-src";
    case CSPDirectiveName::ManifestSrc:
      return "manifest-src";
    case CSPDirectiveName::MediaSrc:
      return "media-src";
    case CSPDirectiveName::ObjectSrc:
      return "object-src";
    case CSPDirectiveName::PrefetchSrc:
      return "prefetch-src";
    case CSPDirectiveName::ReportURI:
      return "report-uri";
    case CSPDirectiveName::Sandbox:
      return "sandbox";
    case CSPDirectiveName::ScriptSrc:
      return "script-src";
    case CSPDirectiveName::ScriptSrcAttr:
      return "script-src-attr";
    case CSPDirectiveName::ScriptSrcElem:
      return "script-src-elem";
    case CSPDirectiveName::StyleSrc:
      return "style-src";
    case CSPDirectiveName::StyleSrcAttr:
      return "style-src-attr";
    case CSPDirectiveName::StyleSrcElem:
      return "style-src-elem";
    case CSPDirectiveName::UpgradeInsecureRequests:
      return "upgrade-insecure-requests";
    case CSPDirectiveName::TreatAsPublicAddress:
      return "treat-as-public-address";
    case CSPDirectiveName::WorkerSrc:
      return "worker-src";
    case CSPDirectiveName::ReportTo:
      return "report-to";
    case CSPDirectiveName::NavigateTo:
      return "navigate-to";
    case CSPDirectiveName::Unknown:
      return "";
  }
  NOTREACHED();
  return "";
}

}  // namespace network
