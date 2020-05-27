// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/url_data_source.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "net/url_request/url_request.h"

namespace {
// A chrome-untrusted data source's name starts with chrome-untrusted://.
bool IsChromeUntrustedDataSource(content::URLDataSource* source) {
  static const base::NoDestructor<std::string> kChromeUntrustedSourceNamePrefix(
      base::StrCat(
          {content::kChromeUIUntrustedScheme, url::kStandardSchemeSeparator}));

  return base::StartsWith(source->GetSource(),
                          *kChromeUntrustedSourceNamePrefix,
                          base::CompareCase::SENSITIVE);
}
}  // namespace

namespace content {

// static
void URLDataSource::Add(BrowserContext* browser_context,
                        std::unique_ptr<URLDataSource> source) {
  URLDataManager::AddDataSource(browser_context, std::move(source));
}

// static
URLDataSource* URLDataSource::GetSourceForURL(BrowserContext* browser_context,
                                              const GURL& url) {
  return URLDataManagerBackend::GetForBrowserContext(browser_context)
      ->GetDataSourceFromURL(url)
      ->source();
}

// static
std::string URLDataSource::URLToRequestPath(const GURL& url) {
  const std::string& spec = url.possibly_invalid_spec();
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  // + 1 to skip the slash at the beginning of the path.
  int offset = parsed.CountCharactersBefore(url::Parsed::PATH, false) + 1;

  if (offset < static_cast<int>(spec.size()))
    return spec.substr(offset);

  return std::string();
}

bool URLDataSource::ShouldReplaceExistingSource() {
  return true;
}

bool URLDataSource::AllowCaching() {
  return true;
}

bool URLDataSource::ShouldAddContentSecurityPolicy() {
  return true;
}

std::string URLDataSource::GetContentSecurityPolicyChildSrc() {
  return "child-src 'none';";
}

std::string URLDataSource::GetContentSecurityPolicyDefaultSrc() {
  return IsChromeUntrustedDataSource(this) ? "default-src 'self';"
                                           : std::string();
}

std::string URLDataSource::GetContentSecurityPolicyImgSrc() {
  return std::string();
}

std::string URLDataSource::GetContentSecurityPolicyObjectSrc() {
  return "object-src 'none';";
}

std::string URLDataSource::GetContentSecurityPolicyScriptSrc() {
  // Note: Do not add 'unsafe-eval' here. Instead override CSP for the
  // specific pages that need it, see context http://crbug.com/525224.
  return "script-src chrome://resources 'self';";
}

std::string URLDataSource::GetContentSecurityPolicyStyleSrc() {
  return std::string();
}

std::string URLDataSource::GetContentSecurityPolicyWorkerSrc() {
  return std::string();
}

std::string URLDataSource::GetContentSecurityPolicyFrameAncestors() {
  return "frame-ancestors 'none';";
}

bool URLDataSource::ShouldDenyXFrameOptions() {
  return true;
}

bool URLDataSource::ShouldServiceRequest(const GURL& url,
                                         BrowserContext* browser_context,
                                         int render_process_id) {
  return url.SchemeIs(kChromeDevToolsScheme) || url.SchemeIs(kChromeUIScheme) ||
         url.SchemeIs(kChromeUIUntrustedScheme);
}

bool URLDataSource::ShouldServeMimeTypeAsContentTypeHeader() {
  return false;
}

std::string URLDataSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) {
  return std::string();
}

void URLDataSource::DisablePolymer2ForHost(const std::string& host) {}

const ui::TemplateReplacements* URLDataSource::GetReplacements() {
  return nullptr;
}

bool URLDataSource::ShouldReplaceI18nInJS() {
  return false;
}

}  // namespace content
