// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/chrome_web_client.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/services/patch/patch_service.h"
#include "components/services/patch/public/mojom/constants.mojom.h"
#include "components/services/unzip/public/mojom/constants.mojom.h"
#include "components/services/unzip/unzip_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_about_rewriter.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ios_chrome_main_parts.h"
#include "ios/chrome/browser/passwords/password_manager_features.h"
#include "ios/chrome/browser/reading_list/features.h"
#import "ios/chrome/browser/reading_list/offline_page_tab_helper.h"
#include "ios/chrome/browser/ssl/ios_ssl_error_handler.h"
#import "ios/chrome/browser/ui/elements/windowed_container_view.h"
#include "ios/chrome/browser/web/chrome_overlay_manifests.h"
#import "ios/chrome/browser/web/error_page_util.h"
#include "ios/public/provider/chrome/browser/browser_url_rewriter_provider.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/voice/audio_session_controller.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#include "ios/web/public/navigation/browser_url_rewriter.h"
#include "ios/web/public/service/service_names.mojom.h"
#include "ios/web/public/user_agent.h"
#include "net/http/http_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name) {
  DCHECK(script_file_name);
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:script_file_name
                                             ofType:@"js"];
  DCHECK(path) << "Script file not found: "
               << base::SysNSStringToUTF8(script_file_name) << ".js";
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error) << "Error fetching script: "
                 << base::SysNSStringToUTF8(error.description);
  DCHECK(content);
  return content;
}
}  // namespace

const char kDesktopUserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_13_5) "
    "AppleWebKit/605.1.15 (KHTML, like Gecko) "
    "Version/11.1.1 "
    "Safari/605.1.15";

ChromeWebClient::ChromeWebClient() {}

ChromeWebClient::~ChromeWebClient() {}

std::unique_ptr<web::WebMainParts> ChromeWebClient::CreateWebMainParts() {
  return std::make_unique<IOSChromeMainParts>(
      *base::CommandLine::ForCurrentProcess());
}

void ChromeWebClient::PreWebViewCreation() const {
  // Initialize the audio session to allow a web page's audio to continue
  // playing after the app is backgrounded.
  VoiceSearchProvider* voice_provider =
      ios::GetChromeBrowserProvider()->GetVoiceSearchProvider();
  if (voice_provider) {
    AudioSessionController* audio_controller =
        voice_provider->GetAudioSessionController();
    if (audio_controller) {
      audio_controller->InitializeSessionIfNecessary();
    }
  }
}

void ChromeWebClient::AddAdditionalSchemes(Schemes* schemes) const {
  schemes->standard_schemes.push_back(kChromeUIScheme);
  schemes->secure_schemes.push_back(kChromeUIScheme);
}

std::string ChromeWebClient::GetApplicationLocale() const {
  DCHECK(GetApplicationContext());
  return GetApplicationContext()->GetApplicationLocale();
}

bool ChromeWebClient::IsAppSpecificURL(const GURL& url) const {
  return url.SchemeIs(kChromeUIScheme);
}

base::string16 ChromeWebClient::GetPluginNotSupportedText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_SUPPORTED);
}

std::string ChromeWebClient::GetUserAgent(web::UserAgentType type) const {
  // The user agent should not be requested for app-specific URLs.
  DCHECK_NE(type, web::UserAgentType::NONE);

  // Using desktop user agent overrides a command-line user agent, so that
  // request desktop site can still work when using an overridden UA.
  if (type == web::UserAgentType::DESKTOP)
    return kDesktopUserAgent;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserAgent)) {
    std::string user_agent =
        command_line->GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(user_agent))
      return user_agent;
    LOG(WARNING) << "Ignored invalid value for flag --" << switches::kUserAgent;
  }

  return web::BuildUserAgentFromProduct(GetProduct());
}

base::string16 ChromeWebClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ChromeWebClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ChromeWebClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

bool ChromeWebClient::IsDataResourceGzipped(int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().IsGzipped(resource_id);
}

base::Optional<service_manager::Manifest>
ChromeWebClient::GetServiceManifestOverlay(base::StringPiece name) {
  if (name == web::mojom::kBrowserServiceName)
    return GetChromeWebBrowserOverlayManifest();
  return base::nullopt;
}

std::vector<service_manager::Manifest>
ChromeWebClient::GetExtraServiceManifests() {
  return GetChromeWebPackagedServicesOverlayManifest().packaged_services;
}

void ChromeWebClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(dom_distiller::kDomDistillerScheme);
}

void ChromeWebClient::PostBrowserURLRewriterCreation(
    web::BrowserURLRewriter* rewriter) {
  rewriter->AddURLRewriter(&WillHandleWebBrowserAboutURL);
  BrowserURLRewriterProvider* provider =
      ios::GetChromeBrowserProvider()->GetBrowserURLRewriterProvider();
  if (provider)
    provider->AddProviderRewriters(rewriter);
}

NSString* ChromeWebClient::GetDocumentStartScriptForAllFrames(
    web::BrowserState* browser_state) const {
  return GetPageScript(@"chrome_bundle_all_frames");
}

NSString* ChromeWebClient::GetDocumentStartScriptForMainFrame(
    web::BrowserState* browser_state) const {
  NSMutableArray* scripts = [NSMutableArray array];
  [scripts addObject:GetPageScript(@"chrome_bundle_main_frame")];

  if (base::FeatureList::IsEnabled(features::kCredentialManager)) {
    [scripts addObject:GetPageScript(@"credential_manager")];
  }

  [scripts addObject:GetPageScript(@"payment_request")];

  return [scripts componentsJoinedByString:@";"];
}

void ChromeWebClient::AllowCertificateError(
    web::WebState* web_state,
    int cert_error,
    const net::SSLInfo& info,
    const GURL& request_url,
    bool overridable,
    const base::Callback<void(bool)>& callback) {
  // TODO(crbug.com/760873): IOSSSLErrorHandler will present an interstitial
  // for the user to decide if it is safe to proceed.
  // Handle the case of web_state not presenting UI to users like prerender tabs
  // or web_state used to fetch offline content in Reading List.
  IOSSSLErrorHandler::HandleSSLError(web_state, cert_error, info, request_url,
                                     overridable, callback);
}

void ChromeWebClient::PrepareErrorPage(web::WebState* web_state,
                                       const GURL& url,
                                       NSError* error,
                                       bool is_post,
                                       bool is_off_the_record,
                                       NSString** error_html) {
  if (reading_list::IsOfflinePageWithoutNativeContentEnabled()) {
    OfflinePageTabHelper* offline_page_tab_helper =
        OfflinePageTabHelper::FromWebState(web_state);
    // WebState that are not attached to a tab may not have a
    // OfflinePageTabHelper.
    if (offline_page_tab_helper &&
        offline_page_tab_helper->HasDistilledVersionForOnlineUrl(url)) {
      // An offline version of the page will be displayed to replace this error
      // page. Return an empty error page to avoid having the error page
      // flash vefore the offline version is loaded.
      *error_html = @"";
      return;
    }
  }
  DCHECK(error);
  *error_html = GetErrorPage(url, error, is_post, is_off_the_record);
}

std::unique_ptr<service_manager::Service> ChromeWebClient::HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (service_name == unzip::mojom::kServiceName) {
    // The Unzip service is used by the component updater.
    return std::make_unique<unzip::UnzipService>(std::move(request));
  }
  if (service_name == patch::mojom::kServiceName) {
    // The Patch service is used by the component updater.
    return std::make_unique<patch::PatchService>(std::move(request));
  }

  return nullptr;
}

UIView* ChromeWebClient::GetWindowedContainer() {
  if (!windowed_container_) {
    windowed_container_ = [[WindowedContainerView alloc] init];
  }
  return windowed_container_;
}

std::string ChromeWebClient::GetProduct() const {
  std::string product("CriOS/");
  product += version_info::GetVersionNumber();
  return product;
}
