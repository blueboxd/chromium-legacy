// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/untrusted_source.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/memory/ref_counted_memory.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_data.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/search/promos/promo_data.h"
#include "chrome/browser/search/promos/promo_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"

namespace {

std::string FormatTemplate(int resource_id,
                           const ui::TemplateReplacements& replacements) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  base::RefCountedMemory* bytes = bundle.LoadDataResourceBytes(resource_id);
  base::StringPiece string_piece(reinterpret_cast<const char*>(bytes->front()),
                                 bytes->size());
  return ui::ReplaceTemplateExpressions(
      string_piece, replacements,
      /* skip_unexpected_placeholder_check= */ true);
}

std::string ReadBackgroundImageData(const base::FilePath& profile_path) {
  std::string data_string;
  base::ReadFileToString(profile_path.AppendASCII("background.jpg"),
                         &data_string);
  return data_string;
}

void ServeBackgroundImageData(content::URLDataSource::GotDataCallback callback,
                              std::string data_string) {
  std::move(callback).Run(base::RefCountedString::TakeString(&data_string));
}

}  // namespace

UntrustedSource::UntrustedSource(Profile* profile)
    : one_google_bar_service_(
          OneGoogleBarServiceFactory::GetForProfile(profile)),
      profile_(profile),
      promo_service_(PromoServiceFactory::GetForProfile(profile)) {
  // |promo_service_| is null in incognito, or when the feature is
  // disabled.
  if (promo_service_) {
    promo_service_observer_.Add(promo_service_);
  }

  // |one_google_bar_service_| is null in incognito, or when the feature is
  // disabled.
  if (one_google_bar_service_) {
    one_google_bar_service_observer_.Add(one_google_bar_service_);
  }
}

UntrustedSource::~UntrustedSource() = default;

std::string UntrustedSource::GetContentSecurityPolicyScriptSrc() {
  return "script-src 'self' 'unsafe-inline' https:;";
}

std::string UntrustedSource::GetContentSecurityPolicyChildSrc() {
  return "child-src https:;";
}

std::string UntrustedSource::GetSource() {
  return chrome::kChromeUIUntrustedNewTabPageUrl;
}

void UntrustedSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  const std::string path = url.has_path() ? url.path().substr(1) : "";
  GURL url_param = GURL(url.query());
  if (path == "one-google-bar" && one_google_bar_service_) {
    std::string ogdeb_value;
    net::GetValueForKeyInQuery(url, "ogdebencoded", &ogdeb_value);
    base::Base64Decode(ogdeb_value, &ogdeb_value);
    bool wait_for_refresh = one_google_bar_service_->SetOgdebValue(ogdeb_value);
    one_google_bar_callbacks_.push_back(std::move(callback));
    if (one_google_bar_service_->one_google_bar_data().has_value() &&
        !wait_for_refresh) {
      OnOneGoogleBarDataUpdated();
    }
    one_google_bar_service_->Refresh();
    return;
  }
  if (path == "one_google_bar.js") {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    std::move(callback).Run(bundle.LoadDataResourceBytes(
        IDR_NEW_TAB_PAGE_UNTRUSTED_ONE_GOOGLE_BAR_JS));
    return;
  }
  if (path == "promo" && promo_service_) {
    promo_callbacks_.push_back(std::move(callback));
    if (promo_service_->promo_data().has_value()) {
      OnPromoDataUpdated();
    }
    promo_service_->Refresh();
    return;
  }
  if (path == "promo.js") {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    std::move(callback).Run(
        bundle.LoadDataResourceBytes(IDR_NEW_TAB_PAGE_UNTRUSTED_PROMO_JS));
    return;
  }
  if ((path == "image" || path == "background_image" || path == "iframe") &&
      url_param.is_valid() &&
      (url_param.SchemeIs(url::kHttpsScheme) ||
       url_param.SchemeIs(content::kChromeUIUntrustedScheme))) {
    ui::TemplateReplacements replacements;
    replacements["url"] = url_param.spec();
    int resource_id =
        (path == "image")
            ? IDR_NEW_TAB_PAGE_UNTRUSTED_IMAGE_HTML
            : (path == "background_image")
                  ? IDR_NEW_TAB_PAGE_UNTRUSTED_BACKGROUND_IMAGE_HTML
                  : IDR_NEW_TAB_PAGE_UNTRUSTED_IFRAME_HTML;
    std::string html = FormatTemplate(resource_id, replacements);
    std::move(callback).Run(base::RefCountedString::TakeString(&html));
    return;
  }
  if (path == "background_image.js") {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    std::move(callback).Run(bundle.LoadDataResourceBytes(
        IDR_NEW_TAB_PAGE_UNTRUSTED_BACKGROUND_IMAGE_JS));
    return;
  }
  if (path == "background.jpg") {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&ReadBackgroundImageData, profile_->GetPath()),
        base::BindOnce(&ServeBackgroundImageData, std::move(callback)));
    return;
  }
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>());
}

std::string UntrustedSource::GetMimeType(const std::string& path) {
  const std::string stripped_path = path.substr(0, path.find("?"));
  if (base::EndsWith(stripped_path, ".js",
                     base::CompareCase::INSENSITIVE_ASCII))
    return "application/javascript";
  if (base::EndsWith(stripped_path, ".jpg",
                     base::CompareCase::INSENSITIVE_ASCII))
    return "image/jpg";

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
  return path == "one-google-bar" || path == "one_google_bar.js" ||
         path == "promo" || path == "promo.js" || path == "image" ||
         path == "background_image" || path == "background_image.js" ||
         path == "iframe" || path == "background.jpg";
}

void UntrustedSource::OnOneGoogleBarDataUpdated() {
  base::Optional<OneGoogleBarData> data =
      one_google_bar_service_->one_google_bar_data();
  std::string html;
  if (data.has_value()) {
    ui::TemplateReplacements replacements;
    replacements["textdirection"] = base::i18n::IsRTL() ? "rtl" : "ltr";
    replacements["barHtml"] = data->bar_html;
    replacements["inHeadScript"] = data->in_head_script;
    replacements["inHeadStyle"] = data->in_head_style;
    replacements["afterBarScript"] = data->after_bar_script;
    replacements["endOfBodyHtml"] = data->end_of_body_html;
    replacements["endOfBodyScript"] = data->end_of_body_script;
    html = FormatTemplate(IDR_NEW_TAB_PAGE_UNTRUSTED_ONE_GOOGLE_BAR_HTML,
                          replacements);
  }
  for (auto& callback : one_google_bar_callbacks_) {
    std::move(callback).Run(base::RefCountedString::TakeString(&html));
  }
  one_google_bar_callbacks_.clear();
}

void UntrustedSource::OnOneGoogleBarServiceShuttingDown() {
  one_google_bar_service_observer_.RemoveAll();
  one_google_bar_service_ = nullptr;
}

void UntrustedSource::OnPromoDataUpdated() {
  const auto& data = promo_service_->promo_data();
  std::string html;
  if (data.has_value() && !data->promo_html.empty()) {
    ui::TemplateReplacements replacements;
    replacements["textdirection"] = base::i18n::IsRTL() ? "rtl" : "ltr";
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
