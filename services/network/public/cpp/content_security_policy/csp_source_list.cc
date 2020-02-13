// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/csp_source_list.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/cpp/content_security_policy/csp_source.h"

namespace network {

namespace {

bool AllowFromSources(const GURL& url,
                      const std::vector<mojom::CSPSourcePtr>& sources,
                      CSPContext* context,
                      bool has_followed_redirect) {
  for (const auto& source : sources) {
    if (CheckCSPSource(source, url, context, has_followed_redirect))
      return true;
  }
  return false;
}

}  // namespace

bool CheckCSPSourceList(const mojom::CSPSourceListPtr& source_list,
                        const GURL& url,
                        CSPContext* context,
                        bool has_followed_redirect,
                        bool is_response_check) {
  // If the source list allows all redirects, the decision can't be made until
  // the response is received.
  if (source_list->allow_response_redirects && !is_response_check)
    return true;

  // Wildcards match network schemes ('http', 'https', 'ftp', 'ws', 'wss'), and
  // the scheme of the protected resource:
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression. Other
  // schemes, including custom schemes, must be explicitly listed in a source
  // list.
  if (source_list->allow_star) {
    if (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsWSOrWSS() ||
        url.SchemeIs("ftp")) {
      return true;
    }
    if (context->self_source() && url.SchemeIs(context->self_source()->scheme))
      return true;
  }

  if (source_list->allow_self && context->self_source() &&
      CheckCSPSource(context->self_source(), url, context,
                     has_followed_redirect)) {
    return true;
  }

  return AllowFromSources(url, source_list->sources, context,
                          has_followed_redirect);
}

std::string ToString(const mojom::CSPSourceListPtr& source_list) {
  bool is_none = !source_list->allow_self && !source_list->allow_star &&
                 source_list->sources.empty();
  if (is_none)
    return "'none'";
  if (source_list->allow_star)
    return "*";

  bool is_empty = true;
  std::stringstream text;
  if (source_list->allow_self) {
    text << "'self'";
    is_empty = false;
  }

  for (const auto& source : source_list->sources) {
    if (!is_empty)
      text << " ";
    text << ToString(source);
    is_empty = false;
  }

  return text.str();
}

}  // namespace network
