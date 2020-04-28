// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/ancestor_throttle.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/origin.h"

namespace content {

namespace {
const char kXFrameOptionsSameOriginHistogram[] = "Security.XFrameOptions";

// This enum is used for UMA metrics. Keep these enums up to date with
// tools/metrics/histograms/histograms.xml.
enum XFrameOptionsHistogram {
  // A frame is loaded without any X-Frame-Options header.
  NONE = 0,

  // X-Frame-Options: DENY.
  DENY = 1,

  // X-Frame-Options: SAMEORIGIN. The navigation proceeds and every ancestor
  // has the same origin.
  SAMEORIGIN = 2,

  // X-Frame-Options: SAMEORIGIN. The navigation is blocked because the
  // top-frame doesn't have the same origin.
  SAMEORIGIN_BLOCKED = 3,

  // X-Frame-Options: SAMEORIGIN. The navigation proceeds despite the fact that
  // there is an ancestor that doesn't have the same origin.
  SAMEORIGIN_WITH_BAD_ANCESTOR_CHAIN = 4,

  // X-Frame-Options: ALLOWALL.
  ALLOWALL = 5,

  // Invalid 'X-Frame-Options' directive encountered.
  INVALID = 6,

  // The frame sets multiple 'X-Frame-Options' header with conflicting values.
  CONFLICT = 7,

  // The 'frame-ancestors' CSP directive should take effect instead.
  BYPASS = 8,

  // Navigation would have been blocked if we applied 'X-Frame-Options' to
  // redirects.
  //
  // TODO(mkwst): Rename this when we make a decision around
  // https://crbug.com/835465.
  REDIRECT_WOULD_BE_BLOCKED = 9,

  XFRAMEOPTIONS_HISTOGRAM_MAX = REDIRECT_WOULD_BE_BLOCKED
};

void RecordXFrameOptionsUsage(XFrameOptionsHistogram usage) {
  UMA_HISTOGRAM_ENUMERATION(
      kXFrameOptionsSameOriginHistogram, usage,
      XFrameOptionsHistogram::XFRAMEOPTIONS_HISTOGRAM_MAX);
}

bool HeadersContainFrameAncestorsCSP(const net::HttpResponseHeaders* headers,
                                     bool include_report_only) {
  std::vector<std::string> header_names = {"content-security-policy"};
  if (include_report_only)
    header_names.push_back("content-security-policy-report-only");
  for (const auto& header : header_names) {
    size_t iter = 0;
    std::string value;
    while (headers->EnumerateHeader(&iter, header, &value)) {
      // A content-security-policy is a semicolon-separated list of directives.
      for (const auto& directive : base::SplitStringPiece(
               value, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
        // The trailing " " is intentional; we'd otherwise match
        // "frame-ancestors-is-not-this-directive".
        if (base::StartsWith(directive, "frame-ancestors ",
                             base::CompareCase::INSENSITIVE_ASCII))
          return true;
      }
    }
  }
  return false;
}

class FrameAncestorCSPContext : public network::CSPContext {
 public:
  FrameAncestorCSPContext(
      RenderFrameHostImpl* navigated_frame,
      const std::vector<network::mojom::ContentSecurityPolicyPtr>& policies)
      : navigated_frame_(navigated_frame) {
    // TODO(arthursonzogni): Refactor CSPContext to its original state, it
    // shouldn't own any ContentSecurityPolicies on its own. This should be
    // defined by the implementation instead. Copies could be avoided here.
    for (auto& policy : policies)
      AddContentSecurityPolicy(mojo::Clone(policy));
  }

 private:
  void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation_params) override {
    return navigated_frame_->ReportContentSecurityPolicyViolation(
        std::move(violation_params));
  }

  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override {
    return navigated_frame_->SchemeShouldBypassCSP(scheme);
  }

  void SanitizeDataForUseInCspViolation(
      bool is_redirect,
      network::mojom::CSPDirectiveName directive,
      GURL* blocked_url,
      network::mojom::SourceLocation* source_location) const override {
    return navigated_frame_->SanitizeDataForUseInCspViolation(
        is_redirect, directive, blocked_url, source_location);
  }

  RenderFrameHostImpl* navigated_frame_;
};

// Returns the parent, including outer delegates in the case of portals.
RenderFrameHostImpl* ParentForAncestorThrottle(RenderFrameHostImpl* frame) {
  return frame->InsidePortal() ? frame->ParentOrOuterDelegateFrame()
                               : frame->GetParent();
}

}  // namespace

// static
std::unique_ptr<NavigationThrottle> AncestorThrottle::MaybeCreateThrottleFor(
    NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return std::unique_ptr<NavigationThrottle>(new AncestorThrottle(handle));
}

AncestorThrottle::~AncestorThrottle() {}

NavigationThrottle::ThrottleCheckResult
AncestorThrottle::WillRedirectRequest() {
  // During a redirect, we don't know which RenderFrameHost we'll end up in,
  // so we can't log reliably to the console. We should be able to work around
  // this iff we decide to ship the redirect-blocking behavior, but for now
  // we'll just skip the console-logging bits to collect metrics.
  NavigationThrottle::ThrottleCheckResult result = ProcessResponseImpl(
      LoggingDisposition::DO_NOT_LOG_TO_CONSOLE, false /* is_response_check */);

  if (result.action() == NavigationThrottle::BLOCK_RESPONSE)
    RecordXFrameOptionsUsage(XFrameOptionsHistogram::REDIRECT_WOULD_BE_BLOCKED);

  // TODO(mkwst): We need to decide whether we'll be able to get away with
  // tightening the XFO check to include redirect responses once we have a
  // feel for the REDIRECT_WOULD_BE_BLOCKED numbers we're collecting above.
  // Until then, we'll allow the response to proceed: https://crbug.com/835465.
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
AncestorThrottle::WillProcessResponse() {
  return ProcessResponseImpl(LoggingDisposition::LOG_TO_CONSOLE,
                             true /* is_response_check */);
}

NavigationThrottle::ThrottleCheckResult AncestorThrottle::ProcessResponseImpl(
    LoggingDisposition logging,
    bool is_response_check) {
  NavigationRequest* request = NavigationRequest::From(navigation_handle());

  bool is_portal =
      request->frame_tree_node()->current_frame_host()->InsidePortal();
  if (request->IsInMainFrame() && !is_portal) {
    // Allow main frame navigations.
    return NavigationThrottle::PROCEED;
  }

  // 204/205 responses and downloads are not sent to the renderer and don't need
  // to be checked.
  if (is_response_check && !request->response_should_be_rendered()) {
    return NavigationThrottle::PROCEED;
  }

  std::string header_value;
  HeaderDisposition disposition =
      ParseHeader(request->GetResponseHeaders(), &header_value);

  switch (disposition) {
    case HeaderDisposition::CONFLICT:
      if (logging == LoggingDisposition::LOG_TO_CONSOLE)
        ParseError(header_value, disposition);
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::CONFLICT);
      return NavigationThrottle::BLOCK_RESPONSE;

    case HeaderDisposition::INVALID:
      if (logging == LoggingDisposition::LOG_TO_CONSOLE)
        ParseError(header_value, disposition);
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::INVALID);
      // TODO(mkwst): Consider failing here.
      break;

    case HeaderDisposition::DENY:
      if (logging == LoggingDisposition::LOG_TO_CONSOLE)
        ConsoleError(disposition);
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::DENY);
      return NavigationThrottle::BLOCK_RESPONSE;

    case HeaderDisposition::SAMEORIGIN: {
      // Block the request when any ancestor is not same-origin.
      RenderFrameHostImpl* parent = request->GetParentFrame();
      url::Origin current_origin =
          url::Origin::Create(navigation_handle()->GetURL());
      while (parent) {
        if (!parent->GetLastCommittedOrigin().IsSameOriginWith(
                current_origin)) {
          RecordXFrameOptionsUsage(XFrameOptionsHistogram::SAMEORIGIN_BLOCKED);
          if (logging == LoggingDisposition::LOG_TO_CONSOLE)
            ConsoleError(disposition);

          // TODO(mkwst): Stop recording this metric once we convince other
          // vendors to follow our lead with XFO: SAMEORIGIN processing.
          //
          // https://crbug.com/250309
          if (parent->GetMainFrame()->GetLastCommittedOrigin().IsSameOriginWith(
                  current_origin)) {
            RecordXFrameOptionsUsage(
                XFrameOptionsHistogram::SAMEORIGIN_WITH_BAD_ANCESTOR_CHAIN);
          }

          return NavigationThrottle::BLOCK_RESPONSE;
        }
        parent = parent->GetParent();
      }
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::SAMEORIGIN);
      break;
    }

    case HeaderDisposition::NONE:
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::NONE);
      break;
    case HeaderDisposition::BYPASS:
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::BYPASS);
      break;
    case HeaderDisposition::ALLOWALL:
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::ALLOWALL);
      break;
  }

  // X-Frame-Option is checked on both redirect and final responses. However,
  // CSP:Frame-ancestor is checked only for the final response.
  if (!is_response_check)
    return NavigationThrottle::PROCEED;

  return EvaluateContentSecurityPolicy(
      request->response()->parsed_headers->content_security_policy);
}

const char* AncestorThrottle::GetNameForLogging() {
  return "AncestorThrottle";
}

AncestorThrottle::AncestorThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

void AncestorThrottle::ParseError(const std::string& value,
                                  HeaderDisposition disposition) {
  DCHECK(disposition == HeaderDisposition::CONFLICT ||
         disposition == HeaderDisposition::INVALID);
  if (!navigation_handle()->GetRenderFrameHost())
    return;  // Some responses won't have a RFH (i.e. 204/205s or downloads).

  std::string message;
  if (disposition == HeaderDisposition::CONFLICT) {
    message = base::StringPrintf(
        "Refused to display '%s' in a frame because it set multiple "
        "'X-Frame-Options' headers with conflicting values "
        "('%s'). Falling back to 'deny'.",
        navigation_handle()->GetURL().spec().c_str(), value.c_str());
  } else {
    message = base::StringPrintf(
        "Invalid 'X-Frame-Options' header encountered when loading '%s': "
        "'%s' is not a recognized directive. The header will be ignored.",
        navigation_handle()->GetURL().spec().c_str(), value.c_str());
  }

  // Log a console error in the parent of the current RenderFrameHost (as
  // the current RenderFrameHost itself doesn't yet have a document).
  auto* frame = static_cast<RenderFrameHostImpl*>(
      navigation_handle()->GetRenderFrameHost());
  ParentForAncestorThrottle(frame)->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, message);
}

void AncestorThrottle::ConsoleError(HeaderDisposition disposition) {
  DCHECK(disposition == HeaderDisposition::DENY ||
         disposition == HeaderDisposition::SAMEORIGIN);
  if (!navigation_handle()->GetRenderFrameHost())
    return;  // Some responses won't have a RFH (i.e. 204/205s or downloads).

  std::string message = base::StringPrintf(
      "Refused to display '%s' in a frame because it set 'X-Frame-Options' "
      "to '%s'.",
      navigation_handle()->GetURL().spec().c_str(),
      disposition == HeaderDisposition::DENY ? "deny" : "sameorigin");

  // Log a console error in the parent of the current RenderFrameHost (as
  // the current RenderFrameHost itself doesn't yet have a document).
  auto* frame = static_cast<RenderFrameHostImpl*>(
      navigation_handle()->GetRenderFrameHost());
  ParentForAncestorThrottle(frame)->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, message);
}

NavigationThrottle::ThrottleAction
AncestorThrottle::EvaluateContentSecurityPolicy(
    const std::vector<network::mojom::ContentSecurityPolicyPtr>&
        content_security_policy) {
  // TODO(lfg): If the initiating document is known and correspond to the
  // navigating frame's current document, consider using:
  // navigation_request().common_params().source_location here instead.
  auto empty_source_location = network::mojom::SourceLocation::New();

  // CSP frame-ancestors are checked against the URL of every parent and are
  // reported to the navigating frame.
  FrameAncestorCSPContext csp_context(
      NavigationRequest::From(navigation_handle())->GetRenderFrameHost(),
      content_security_policy);
  csp_context.SetSelf(url::Origin::Create(navigation_handle()->GetURL()));

  // Check CSP frame-ancestors against every parent.
  // We enforce frame-ancestors in the outer delegate for portals, but not
  // for other uses of inner/outer WebContents (GuestViews).
  RenderFrameHostImpl* parent =
      ParentForAncestorThrottle(static_cast<RenderFrameHostImpl*>(
          navigation_handle()->GetRenderFrameHost()));
  while (parent) {
    if (!csp_context.IsAllowedByCsp(
            network::mojom::CSPDirectiveName::FrameAncestors,
            parent->GetLastCommittedOrigin().GetURL(),
            navigation_handle()->WasServerRedirect(),
            true /* is_response_check */, empty_source_location,
            network::CSPContext::CheckCSPDisposition::CHECK_ALL_CSP,
            navigation_handle()->IsFormSubmission())) {
      return NavigationThrottle::BLOCK_RESPONSE;
    }
    parent = ParentForAncestorThrottle(parent);
  }

  return NavigationThrottle::PROCEED;
}

AncestorThrottle::HeaderDisposition AncestorThrottle::ParseHeader(
    const net::HttpResponseHeaders* headers,
    std::string* header_value) {
  DCHECK(header_value);
  if (!headers)
    return HeaderDisposition::NONE;

  // Process the 'X-Frame-Options header as per Section 2 of RFC7034:
  // https://tools.ietf.org/html/rfc7034#section-2
  //
  // Note that we do not support the 'ALLOW-FROM' value, and we special-case
  // the invalid "ALLOWALL" value due to its prevalance in the wild.
  HeaderDisposition result = HeaderDisposition::NONE;
  size_t iter = 0;
  std::string value;
  while (headers->EnumerateHeader(&iter, "x-frame-options", &value)) {
    HeaderDisposition current = HeaderDisposition::INVALID;

    base::StringPiece trimmed =
        base::TrimWhitespaceASCII(value, base::TRIM_ALL);
    if (!header_value->empty())
      header_value->append(", ");
    header_value->append(trimmed.as_string());

    if (base::LowerCaseEqualsASCII(trimmed, "deny"))
      current = HeaderDisposition::DENY;
    else if (base::LowerCaseEqualsASCII(trimmed, "allowall"))
      current = HeaderDisposition::ALLOWALL;
    else if (base::LowerCaseEqualsASCII(trimmed, "sameorigin"))
      current = HeaderDisposition::SAMEORIGIN;
    else
      current = HeaderDisposition::INVALID;

    if (result == HeaderDisposition::NONE)
      result = current;
    else if (result != current)
      result = HeaderDisposition::CONFLICT;
  }

  // If 'X-Frame-Options' would potentially block the response, check whether
  // the 'frame-ancestors' CSP directive should take effect instead. See
  // https://www.w3.org/TR/CSP/#frame-ancestors-and-frame-options
  if (result != HeaderDisposition::NONE &&
      result != HeaderDisposition::ALLOWALL &&
      HeadersContainFrameAncestorsCSP(headers, false)) {
    // TODO(mkwst): 'frame-ancestors' is currently handled in Blink. We should
    // handle it here instead. Until then, don't block the request, and let
    // Blink handle it. https://crbug.com/555418
    return HeaderDisposition::BYPASS;
  }
  return result;
}

}  // namespace content
