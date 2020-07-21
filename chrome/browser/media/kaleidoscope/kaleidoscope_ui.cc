// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_ui.h"

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/media/kaleidoscope/constants.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_data_provider_impl.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(ENABLE_KALEIDOSCOPE)
#include "chrome/browser/media/kaleidoscope/grit/kaleidoscope_resources.h"
#endif  // BUILDFLAG(ENABLE_KALEIDOSCOPE)

namespace {

// Wraps the strings in JS so they can be accessed by the code. The strings are
// placed on the window object so they can always be accessed.
const char kStringWrapper[] =
    "window.KALEIDOSCOPE_STRINGS = new Map(Object.entries(%s));";

bool OnShouldHandleRequest(const std::string& path) {
  return base::EqualsCaseInsensitiveASCII(path,
                                          "resources/_locales/strings.js");
}

void OnStringsRequest(const std::string& path,
                      content::WebUIDataSource::GotDataCallback callback) {
  DCHECK(OnShouldHandleRequest(path));

#if BUILDFLAG(ENABLE_KALEIDOSCOPE)
  // TODO(beccahughes): Switch locale here.
  auto str = ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_KALEIDOSCOPE_LOCALE_EN);
#else
  std::string str;
#endif  // BUILDFLAG(ENABLE_KALEIDOSCOPE)

  base::RefCountedString* ref_contents = new base::RefCountedString();
  ref_contents->data() = base::StringPrintf(kStringWrapper, str.c_str());
  std::move(callback).Run(ref_contents);
}

content::WebUIDataSource* CreateUntrustedWebUIDataSource() {
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::Create(kKaleidoscopeUntrustedContentUIURL);
  untrusted_source->DisableDenyXFrameOptions();
  untrusted_source->UseStringsJs();

  // Add a request filter to handle strings.js
  untrusted_source->SetRequestFilter(base::BindRepeating(OnShouldHandleRequest),
                                     base::BindRepeating(OnStringsRequest));

  const auto backend_url =
      GetGoogleAPIBaseURL(*base::CommandLine::ForCurrentProcess());

  // Allow scripts and styles from chrome-untrusted://resources.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome-untrusted://resources 'unsafe-inline' 'self';");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src chrome-untrusted://resources 'unsafe-inline' 'self';");

  // Allow images and videos from anywhere.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src * data:;");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc, "media-src *;");

  // Allow access to Google APIs.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      base::StringPrintf("connect-src %s;", backend_url.spec().c_str()));

  // Add the URL to the backend.
  untrusted_source->AddString("googleApiUrl", backend_url.spec());

#if BUILDFLAG(ENABLE_KALEIDOSCOPE)
  untrusted_source->AddResourcePath("content.css",
                                    IDR_KALEIDOSCOPE_CONTENT_CSS);
  untrusted_source->AddResourcePath("content.js", IDR_KALEIDOSCOPE_CONTENT_JS);
  untrusted_source->AddResourcePath("messages.js",
                                    IDR_KALEIDOSCOPE_MESSAGES_JS);
  untrusted_source->AddResourcePath("toolbar.js", IDR_KALEIDOSCOPE_TOOLBAR_JS);
  untrusted_source->AddResourcePath("side_nav_container.js",
                                    IDR_KALEIDOSCOPE_SIDE_NAV_CONTAINER_JS);

  untrusted_source->AddResourcePath("geometry.mojom-lite.js",
                                    IDR_GEOMETRY_MOJOM_LITE_JS);
  untrusted_source->AddResourcePath("kaleidoscope.mojom-lite.js",
                                    IDR_KALEIDOSCOPE_MOJOM_LITE_JS);
  untrusted_source->AddResourcePath(
      "chrome/browser/media/feeds/media_feeds_store.mojom-lite.js",
      IDR_MEDIA_FEEDS_STORE_MOJOM_LITE_JS);

  // Google Sans.
  untrusted_source->AddResourcePath("resources/fonts/fonts.css",
                                    IDR_GOOGLE_SANS_CSS);
  untrusted_source->AddResourcePath("resources/fonts/GoogleSans-Bold.woff2",
                                    IDR_GOOGLE_SANS_BOLD);
  untrusted_source->AddResourcePath("resources/fonts/GoogleSans-Medium.woff2",
                                    IDR_GOOGLE_SANS_MEDIUM);
  untrusted_source->AddResourcePath("resources/fonts/GoogleSans-Regular.woff2",
                                    IDR_GOOGLE_SANS_REGULAR);

  untrusted_source->AddResourcePath("content.html",
                                    IDR_KALEIDOSCOPE_CONTENT_HTML);
#endif  // BUILDFLAG(ENABLE_KALEIDOSCOPE)

  return untrusted_source;
}

content::WebUIDataSource* CreateWebUIDataSource() {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(kKaleidoscopeUIHost);

  // Allows us to put content in an IFrame.
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src chrome-untrusted://kaleidoscope;");

  // Add a request filter to handle strings.js
  html_source->SetRequestFilter(base::BindRepeating(OnShouldHandleRequest),
                                base::BindRepeating(OnStringsRequest));

#if BUILDFLAG(ENABLE_KALEIDOSCOPE)
  html_source->AddResourcePath("kaleidoscope.js", IDR_KALEIDOSCOPE_JS);
  html_source->AddResourcePath("messages.js", IDR_KALEIDOSCOPE_MESSAGES_JS);
  html_source->AddResourcePath("utils.js", IDR_KALEIDOSCOPE_UTILS_JS);

  html_source->AddResourcePath("geometry.mojom-lite.js",
                               IDR_GEOMETRY_MOJOM_LITE_JS);
  html_source->AddResourcePath("kaleidoscope.mojom-lite.js",
                               IDR_KALEIDOSCOPE_MOJOM_LITE_JS);
  html_source->AddResourcePath(
      "chrome/browser/media/feeds/media_feeds_store.mojom-lite.js",
      IDR_MEDIA_FEEDS_STORE_MOJOM_LITE_JS);
  html_source->SetDefaultResource(IDR_KALEIDOSCOPE_HTML);
#endif  // BUILDFLAG(ENABLE_KALEIDOSCOPE)

  return html_source;
}

}  // anonymous namespace

// We set |enable_chrome_send| to true since we need it for browser tests.
KaleidoscopeUI::KaleidoscopeUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, CreateWebUIDataSource());
  content::WebUIDataSource::Add(browser_context,
                                CreateUntrustedWebUIDataSource());
}

KaleidoscopeUI::~KaleidoscopeUI() = default;

void KaleidoscopeUI::BindInterface(
    mojo::PendingReceiver<media::mojom::KaleidoscopeDataProvider> provider) {
  provider_ = std::make_unique<KaleidoscopeDataProviderImpl>(
      std::move(provider), Profile::FromWebUI(web_ui()));
}

WEB_UI_CONTROLLER_TYPE_IMPL(KaleidoscopeUI)
