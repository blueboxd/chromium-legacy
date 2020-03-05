// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/untrusted_source.h"

#include <string>
#include <utility>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/promos/promo_data.h"
#include "chrome/browser/search/promos/promo_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "content/public/common/url_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"

namespace {

std::string FormatTemplate(int resource_id,
                           const ui::TemplateReplacements& replacements) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  base::RefCountedMemory* bytes = bundle.LoadDataResourceBytes(resource_id);
  base::StringPiece string_piece(reinterpret_cast<const char*>(bytes->front()),
                                 bytes->size());
  return ui::ReplaceTemplateExpressions(string_piece, replacements);
}

}  // namespace

UntrustedSource::UntrustedSource(Profile* profile)
    : promo_service_(PromoServiceFactory::GetForProfile(profile)) {
  // |promo_service_| is null in incognito, or when the feature is
  // disabled.
  if (promo_service_) {
    promo_service_observer_.Add(promo_service_);
  }
}

UntrustedSource::~UntrustedSource() = default;

std::string UntrustedSource::GetContentSecurityPolicyScriptSrc() {
  return "script-src 'self' 'unsafe-inline';";
}

std::string UntrustedSource::GetSource() {
  return chrome::kChromeUIUntrustedNewTabPageUrl;
}

void UntrustedSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  const std::string path = url.has_path() ? url.path().substr(1) : "";
  if (path == "promo" && promo_service_) {
    promo_callbacks_.push_back(std::move(callback));
    if (promo_callbacks_.size() == 1) {
      promo_service_->Refresh();
    }
    return;
  }
  if (path == "promo.js") {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    std::move(callback).Run(
        bundle.LoadDataResourceBytes(IDR_NEW_TAB_PAGE_UNTRUSTED_PROMO_JS));
    return;
  }
  if (path == "image" && url.has_query()) {
    ui::TemplateReplacements replacements;
    replacements["url"] = url.query();
    std::string html =
        FormatTemplate(IDR_NEW_TAB_PAGE_UNTRUSTED_IMAGE_HTML, replacements);
    std::move(callback).Run(base::RefCountedString::TakeString(&html));
    return;
  }
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>());
}

std::string UntrustedSource::GetMimeType(const std::string& path) {
  if (base::EndsWith(path, ".js", base::CompareCase::INSENSITIVE_ASCII))
    return "application/javascript";

  return "text/html";
}

bool UntrustedSource::AllowCaching() {
  return false;
}

std::string UntrustedSource::GetContentSecurityPolicyFrameAncestors() {
  return base::StringPrintf("frame-ancestors %s",
                            chrome::kChromeUINewTabPageURL);
}

bool UntrustedSource::ShouldReplaceExistingSource() {
  return false;
}

bool UntrustedSource::ShouldServiceRequest(
    const GURL& url,
    content::ResourceContext* resource_context,
    int render_process_id) {
  if (!url.SchemeIs(content::kChromeUIUntrustedScheme) || !url.has_path()) {
    return false;
  }
  const std::string path = url.path().substr(1);
  return path == "promo" || path == "promo.js" || path == "image";
}

void UntrustedSource::OnPromoDataUpdated() {
  const auto& data = promo_service_->promo_data();
  std::string html;
  if (data.has_value() && !data->promo_html.empty()) {
    ui::TemplateReplacements replacements;
    replacements["data"] = data->promo_html;
    html = FormatTemplate(IDR_NEW_TAB_PAGE_UNTRUSTED_PROMO_HTML, replacements);
  }
  for (auto& callback : promo_callbacks_) {
    std::move(callback).Run(base::RefCountedString::TakeString(&html));
  }
  promo_callbacks_.clear();
}

void UntrustedSource::OnPromoServiceShuttingDown() {
  promo_service_observer_.RemoveAll();
  promo_service_ = nullptr;
}
