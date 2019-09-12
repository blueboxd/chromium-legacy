// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/url_request_rewrite_rules_manager.h"

#include "fuchsia/base/string_util.h"
#include "fuchsia/engine/url_request_rewrite_type_converters.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace {

using RoutingIdRewriterMap =
    std::unordered_map<int, UrlRequestRewriteRulesManager*>;
RoutingIdRewriterMap& GetRewriterMap() {
  static base::NoDestructor<RoutingIdRewriterMap> rewriter_map;
  return *rewriter_map;
}

bool ValidateAddHeaders(
    const fuchsia::web::UrlRequestRewriteAddHeaders& add_headers) {
  if (!add_headers.has_headers())
    return false;
  for (const auto& header : add_headers.headers()) {
    base::StringPiece header_name = cr_fuchsia::BytesAsString(header.name);
    base::StringPiece header_value = cr_fuchsia::BytesAsString(header.value);
    if (!net::HttpUtil::IsValidHeaderName(header_name) ||
        !net::HttpUtil::IsValidHeaderValue(header_value))
      return false;
  }
  return true;
}

bool ValidateRemoveHeader(
    const fuchsia::web::UrlRequestRewriteRemoveHeader& remove_header) {
  if (!remove_header.has_header_name())
    return false;
  if (!net::HttpUtil::IsValidHeaderName(
          cr_fuchsia::BytesAsString(remove_header.header_name())))
    return false;
  return true;
}

bool ValidateSubstituteQueryPattern(
    const fuchsia::web::UrlRequestRewriteSubstituteQueryPattern&
        substitute_query_pattern) {
  if (!substitute_query_pattern.has_pattern() ||
      !substitute_query_pattern.has_substitution())
    return false;
  return true;
}

bool ValidateReplaceUrl(
    const fuchsia::web::UrlRequestRewriteReplaceUrl& replace_url) {
  if (!replace_url.has_url_ends_with() || !replace_url.has_new_url())
    return false;
  if (!GURL("http://site.com/" + replace_url.url_ends_with()).is_valid())
    return false;
  if (!GURL(replace_url.new_url()).is_valid())
    return false;
  return true;
}

bool ValidateRewrite(const fuchsia::web::UrlRequestRewrite& rewrite) {
  switch (rewrite.Which()) {
    case fuchsia::web::UrlRequestRewrite::Tag::kAddHeaders:
      return ValidateAddHeaders(rewrite.add_headers());
    case fuchsia::web::UrlRequestRewrite::Tag::kRemoveHeader:
      return ValidateRemoveHeader(rewrite.remove_header());
    case fuchsia::web::UrlRequestRewrite::Tag::kSubstituteQueryPattern:
      return ValidateSubstituteQueryPattern(rewrite.substitute_query_pattern());
    case fuchsia::web::UrlRequestRewrite::Tag::kReplaceUrl:
      return ValidateReplaceUrl(rewrite.replace_url());
    default:
      // This is to prevent build breakage when adding new rewrites to the FIDL
      // definition. This can also happen if the client sends an empty rewrite,
      // which is invalid.
      return false;
  }
}

bool ValidateRules(
    const std::vector<fuchsia::web::UrlRequestRewriteRule>& rules) {
  for (const auto& rule : rules) {
    if (rule.has_hosts_filter() && rule.hosts_filter().empty())
      return false;
    if (rule.has_schemes_filter() && rule.schemes_filter().empty())
      return false;
    if (!rule.has_rewrites())
      return false;
    for (const auto& rewrite : rule.rewrites()) {
      if (!ValidateRewrite(rewrite))
        return false;
    }
  }
  return true;
}

}  // namespace

// static
UrlRequestRewriteRulesManager*
UrlRequestRewriteRulesManager::ForFrameTreeNodeId(int frame_tree_node_id) {
  auto iter = GetRewriterMap().find(frame_tree_node_id);
  if (iter == GetRewriterMap().end())
    return nullptr;
  return iter->second;
}

// static
std::unique_ptr<UrlRequestRewriteRulesManager>
UrlRequestRewriteRulesManager::CreateForTesting() {
  return base::WrapUnique(new UrlRequestRewriteRulesManager());
}

UrlRequestRewriteRulesManager::UrlRequestRewriteRulesManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

UrlRequestRewriteRulesManager::~UrlRequestRewriteRulesManager() = default;

zx_status_t UrlRequestRewriteRulesManager::OnRulesUpdated(
    std::vector<fuchsia::web::UrlRequestRewriteRule> rules,
    fuchsia::web::Frame::SetUrlRequestRewriteRulesCallback callback) {
  if (!ValidateRules(rules)) {
    base::AutoLock auto_lock(lock_);
    cached_rules_ = nullptr;
    return ZX_ERR_INVALID_ARGS;
  }

  base::AutoLock auto_lock(lock_);
  cached_rules_ =
      base::MakeRefCounted<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>(
          mojo::ConvertTo<std::vector<mojom::UrlRequestRewriteRulePtr>>(
              std::move(rules)));

  // Send the updated rules to the receivers.
  for (const auto& receiver_pair : rules_receivers_per_frame_id_) {
    receiver_pair.second->OnRulesUpdated(mojo::Clone(cached_rules_->data));
  }

  // TODO(crbug.com/976975): Only call the callback when there are pending
  // throttles.
  std::move(callback)();
  return ZX_OK;
}

scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
UrlRequestRewriteRulesManager::GetCachedRules() {
  base::AutoLock auto_lock(lock_);
  return cached_rules_;
}

UrlRequestRewriteRulesManager::UrlRequestRewriteRulesManager() {}

void UrlRequestRewriteRulesManager::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  int frame_tree_node_id = render_frame_host->GetFrameTreeNodeId();

  if (rules_receivers_per_frame_id_.find(frame_tree_node_id) !=
      rules_receivers_per_frame_id_.end()) {
    // This happens on cross-process navigations. It is not necessary to refresh
    // the global map in this case as RenderFrameDeleted will not have been
    // called for this RenderFrameHost.
    size_t deleted = rules_receivers_per_frame_id_.erase(frame_tree_node_id);
    DCHECK(deleted == 1);
  } else {
    // Register this instance of UrlRequestRewriteRulesManager as the URL
    // request rewriter handler for this RenderFrameHost ID.
    auto iter =
        GetRewriterMap().emplace(std::make_pair(frame_tree_node_id, this));
    DCHECK(iter.second);
  }

  // Register the frame rules receiver.
  mojom::UrlRequestRulesReceiverAssociatedPtr rules_receiver;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &rules_receiver);
  auto iter = rules_receivers_per_frame_id_.emplace(frame_tree_node_id,
                                                    std::move(rules_receiver));
  DCHECK(iter.second);

  base::AutoLock auto_lock(lock_);
  if (cached_rules_) {
    // Send an initial set of rules.
    iter.first->second->OnRulesUpdated(mojo::Clone(cached_rules_->data));
  }
}

void UrlRequestRewriteRulesManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  int frame_tree_node_id = render_frame_host->GetFrameTreeNodeId();

  size_t deleted = GetRewriterMap().erase(frame_tree_node_id);
  DCHECK(deleted == 1);

  deleted = rules_receivers_per_frame_id_.erase(frame_tree_node_id);
  DCHECK(deleted == 1);
}
