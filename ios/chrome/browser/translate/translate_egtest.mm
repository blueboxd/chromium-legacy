// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/common/translate_constants.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/translate/translate_app_interface.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/translate/legacy_translate_infobar_constants.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_view_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/components/webui/web_ui_url_constants.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::CloseButton;
using chrome_test_util::ToolsMenuView;
using chrome_test_util::WebStateScrollViewMatcher;

namespace {

// Paragraph height height for test pages. This must be large enough to trigger
// the fullscreen mode.
const int kParagraphHeightEM = 200;

// Some text in English language.
const char kEnglishText[] =
    "After flying to an altitude of 39,045 meters (128,100 feet) in a "
    "helium-filled balloon, Felix Baumgartner completed a record breaking jump "
    "for the ages from the edge of space, exactly 65 years after Chuck Yeager "
    "first broke the sound barrier flying in an experimental rocket-powered "
    "airplane. Felix reached a maximum of speed of 1,342.8 km/h (833mph) "
    "through the near vacuum of the stratosphere before being slowed by the "
    "atmosphere later during his 4:20 minute long freefall. The 43-year-old "
    "Austrian skydiving expert also broke two other world records (highest "
    "freefall, highest manned balloon flight), leaving the one for the longest "
    "freefall to project mentor Col. Joe Kittinger.";

// Some text in French language.
const char kFrenchText[] =
    "Des yeux qui font baisser les miens. Un rire qui se perd sur sa bouche."
    "Voilà le portrait sans retouches de l'homme auquel j'appartiens "
    "Quand il me prend dans ses bras Il me parle tout bas "
    "Je vois la vie en rose Il me dit des mots d'amour "
    "Des mots de tous les jours Et ça me fait quelque chose "
    "Il est entré dans mon cœur Une part de bonheur Dont je connais la cause "
    "C'est lui pour moi, moi pour lui, dans la vie "
    "Il me l'a dit, l'a juré, pour la vie Et dès que je l'aperçois "
    "Alors je sens en moi, Mon cœur qui bat Des nuits d'amour à plus finir "
    "Un grand bonheur qui prend sa place Les ennuis, les chagrins s'effacent "
    "Heureux, heureux à en mourir Quand il me prend dans ses bras "
    "Il me parle tout bas Je vois la vie en rose Il me dit des mots d'amour "
    "Des mots de tous les jours Et ça me fait quelque chose "
    "Il est entré dans mon cœur Une part de bonheur Dont je connais la cause "
    "C'est toi pour moi, moi pour toi, dans la vie "
    "Tu me l'as dit, l'as juré, pour la vie Et dès que je t'aperçois "
    "Alors je sens en moi Mon cœur qui bat";

// Various HTML tags.
const char kHtmlAttribute[] = "<html>";
const char kHtmlAttributeWithDeLang[] = "<html lang=\"de\">";
const char kMetaNotranslateContent[] =
    "<meta name=\"google\" content=\"notranslate\">";
const char kMetaNotranslateValue[] =
    "<meta name=\"google\" value=\"notranslate\">";
const char kMetaItContentLanguage[] =
    "<meta http-equiv=\"content-language\" content=\"it\">";

// Various link components.
// TODO(crbug.com/729195): Re-write the hardcoded address.
const char kHttpServerDomain[] = "127.0.0.1";
const char kLanguagePath[] = "/languagepath/";
const char kLinkPath[] = "/linkpath/";
const char kSubresourcePath[] = "/subresourcepath/";
const char kSomeLanguageUrl[] = "http://languagepath/?http=es";
const char kFrenchPagePath[] = "/frenchpage/";
const char kFrenchPageWithLinkPath[] = "/frenchpagewithlink/";
const char kFrenchPageNoTranslateContent[] = "/frenchpagenotranslatecontent/";
const char kFrenchPageNoTranslateValue[] = "/frenchpagenotranslatevalue/";
const char kTranslateScriptPath[] = "/translatescript/";
const char kTranslateScript[] = "Fake Translate Script";

// Body text for /languagepath/.
const char kLanguagePathText[] = "Some text here.";

// Builds a HTML document with a French text and the given |html| and |meta|
// tags.
std::string GetFrenchPageHtml(const std::string& html_tag,
                              const std::string& meta_tags) {
  return html_tag + meta_tags + "<body>" +
         base::StringPrintf("<p style='height:%dem'>%s</p>", kParagraphHeightEM,
                            kFrenchText) +
         "</body></html>";
}

// Returns a matcher for the translate infobar view.
id<GREYMatcher> TranslateInfobar() {
  return grey_accessibilityID(kTranslateInfobarViewId);
}

// Returns a matcher for the translate infobar's options button.
id<GREYMatcher> OptionsButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_TRANSLATE_INFOBAR_OPTIONS_ACCESSIBILITY_LABEL);
}

// Returns a matcher for the translate options menu.
id<GREYMatcher> OptionsMenu() {
  return grey_accessibilityID(kTranslateOptionsPopupMenuId);
}

// Returns a matcher for the language selection menu.
id<GREYMatcher> LanguagesMenu() {
  return grey_accessibilityID(kLanguageSelectorPopupMenuId);
}

// Returns a matcher for the "More Languages" entry in translate options menu.
id<GREYMatcher> MoreLanguages() {
  return ButtonWithAccessibilityLabelId(
      IDS_TRANSLATE_INFOBAR_OPTIONS_MORE_LANGUAGE);
}

// Returns a matcher for the "Always translate ..." entry in translate options
// menu.
id<GREYMatcher> AlwaysTranslate(NSString* language) {
  return ButtonWithAccessibilityLabel(
      l10n_util::GetNSStringF(IDS_TRANSLATE_INFOBAR_OPTIONS_ALWAYS,
                              base::SysNSStringToUTF16(language)));
}

// Returns a matcher for the "Never translate ..." entry in translate options
// menu.
id<GREYMatcher> NeverTranslate(NSString* language) {
  return ButtonWithAccessibilityLabel(l10n_util::GetNSStringF(
      IDS_TRANSLATE_INFOBAR_OPTIONS_NEVER_TRANSLATE_LANG,
      base::SysNSStringToUTF16(language)));
}

// Returns a matcher for the "Never translate this site" entry in translate
// options menu.
id<GREYMatcher> NeverTranslateSite() {
  return ButtonWithAccessibilityLabel(l10n_util::GetNSString(
      IDS_TRANSLATE_INFOBAR_OPTIONS_NEVER_TRANSLATE_SITE));
}

// Returns a matcher for the "Page not in ..." entry in translate options menu.
id<GREYMatcher> PageNotIn(NSString* language) {
  return ButtonWithAccessibilityLabel(
      l10n_util::GetNSStringF(IDS_TRANSLATE_INFOBAR_OPTIONS_NOT_SOURCE_LANGUAGE,
                              base::SysNSStringToUTF16(language)));
}

// Returns a matcher for the notification snackbar's "UNDO" button.
id<GREYMatcher> UndoButton() {
  return ButtonWithAccessibilityLabelId(IDS_TRANSLATE_NOTIFICATION_UNDO);
}

// Returns a matcher for the Translate manual trigger button in the tools menu.
id<GREYMatcher> toolsMenuTranslateButton() {
  return grey_allOf(grey_accessibilityID(kToolsMenuTranslateId),
                    grey_interactable(), nil);
}

// Returns a matcher for an element with or without the
// UIAccessibilityTraitSelected accessibility trait depending on |selected|.
id<GREYMatcher> ElementIsSelected(BOOL selected) {
  return grey_allOf(
      grey_sufficientlyVisible(),
      selected
          ? grey_accessibilityTrait(UIAccessibilityTraitSelected)
          : grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected)),
      nil);
}

#pragma mark - TestResponseProvider

// A ResponseProvider that provides html responses of texts in different
// languages or links.
class TestResponseProvider : public web::DataResponseProvider {
 public:
  // TestResponseProvider implementation.
  bool CanHandleRequest(const Request& request) override;
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override;

 private:
  // Generates a page with a HTTP "Content-Language" header and "httpEquiv" meta
  // tag.
  // The URL in |request| has two parameters, "http" and "meta", that can be
  // used to set the values of the header and the meta tag. For example:
  // http://someurl?http=en&meta=fr generates a page with a "en" HTTP header and
  // a "fr" meta tag.
  void GetLanguageResponse(const Request& request,
                           scoped_refptr<net::HttpResponseHeaders>* headers,
                           std::string* response_body);
};

bool TestResponseProvider::CanHandleRequest(const Request& request) {
  const GURL& url = request.url;
  return (url.host() == kHttpServerDomain &&
          (url.path() == kLanguagePath || url.path() == kLinkPath ||
           url.path() == kSubresourcePath || url.path() == kFrenchPagePath ||
           url.path() == kFrenchPageWithLinkPath ||
           url.path() == kFrenchPageNoTranslateContent ||
           url.path() == kFrenchPageNoTranslateValue ||
           url.path() == kTranslateScriptPath)) ||
         url.SchemeIs(kChromeUIScheme);
}

void TestResponseProvider::GetResponseHeadersAndBody(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  const GURL& url = request.url;
  *headers = web::ResponseProvider::GetDefaultResponseHeaders();
  if (url.SchemeIs(kChromeUIScheme)) {
    *response_body = url.spec();
    return;
  } else if (url.path() == kLanguagePath) {
    // HTTP header and meta tag read from parameters.
    return GetLanguageResponse(request, headers, response_body);
  } else if (url.path() == kSubresourcePath) {
    // Different "Content-Language" headers in the main page and subresource.
    (*headers)->AddHeader("Content-Language", "fr");
    *response_body = base::StringPrintf(
        "<html><body><img src=%s></body></html>", kSomeLanguageUrl);
    return;
  } else if (url.path() == kLinkPath) {
    // Link to a page with "Content Language" headers.
    GURL url = web::test::HttpServer::MakeUrl(kSomeLanguageUrl);
    *response_body = base::StringPrintf(
        "<html><body><a href='%s' id='click'>Click</a></body></html>",
        url.spec().c_str());
    return;
  } else if (url.path() == kFrenchPagePath) {
    *response_body = GetFrenchPageHtml(kHtmlAttribute, "");
    return;
  } else if (url.path() == kFrenchPageWithLinkPath) {
    GURL page_path_url = web::test::HttpServer::MakeUrl(
        base::StringPrintf("http://%s", kFrenchPagePath));
    *response_body = base::StringPrintf(
        "<html><body>%s<br/><a href='%s' id='link'>link</a></body></html>",
        kFrenchText, page_path_url.spec().c_str());
    return;
  } else if (url.path() == kFrenchPageNoTranslateContent) {
    GURL page_path_url = web::test::HttpServer::MakeUrl(
        base::StringPrintf("http://%s", kFrenchPagePath));
    // A page with French text and a 'content' attribute with "notranslate".
    *response_body = GetFrenchPageHtml(kHtmlAttribute, kMetaNotranslateContent);
    return;
  } else if (url.path() == kFrenchPageNoTranslateValue) {
    GURL page_path_url = web::test::HttpServer::MakeUrl(
        base::StringPrintf("http://%s", kFrenchPagePath));
    // A page with French text and a 'value' attribute with "notranslate".
    *response_body = GetFrenchPageHtml(kHtmlAttribute, kMetaNotranslateValue);
    return;
  } else if (url.path() == kTranslateScriptPath) {
    *response_body = kTranslateScript;
    return;
  }
  NOTREACHED();
}

void TestResponseProvider::GetLanguageResponse(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  const GURL& url = request.url;
  // HTTP headers.
  std::string http;
  net::GetValueForKeyInQuery(url, "http", &http);
  if (!http.empty())
    (*headers)->AddHeader("Content-Language", http);
  // Response body.
  std::string meta;
  net::GetValueForKeyInQuery(url, "meta", &meta);
  *response_body = "<html>";
  if (!meta.empty()) {
    *response_body +=
        "<head>"
        "<meta http-equiv='content-language' content='" +
        meta +
        "'>"
        "</head>";
  }
  *response_body +=
      base::StringPrintf("<html><body>%s</body></html>", kLanguagePathText);
}

}  // namespace

#pragma mark - TranslateTestCase

// Tests for translate.
@interface TranslateTestCase : ChromeTestCase
@end

@implementation TranslateTestCase

- (void)setUp {
  [super setUp];

  // Set up the fake URL for the translate script to hit the mock HTTP server.
  GURL translateScriptURL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kTranslateScriptPath));
  NSString* translateScriptSwitchValue =
      base::SysUTF8ToNSString(translateScriptURL.spec());
  [TranslateAppInterface setUpWithScriptServer:translateScriptSwitchValue];
}

- (void)tearDown {
  [TranslateAppInterface tearDown];
  [super tearDown];
}

#pragma mark - Test Cases

// Tests that different language signals are detected correcty.
- (void)testLanguageDetection {
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://scenarioLanguageDetection");
  std::map<GURL, std::string> responses;
  // A page with French text, German "lang" attribute and Italian content
  // language.
  responses[URL] =
      GetFrenchPageHtml(kHtmlAttributeWithDeLang, kMetaItContentLanguage);
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  [self assertContentLanguage:@"it"
             htmlRootLanguage:@"de"
              adoptedLanguage:base::SysUTF8ToNSString(
                                  translate::kUnknownLanguageCode)];
}

// Tests that hidden text is not considered during detection.
- (void)testLanguageDetectionIgnoreHiddenText {
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://scenarioLanguageDetectionIgnoreHiddenText");
  std::map<GURL, std::string> responses;
  // A page with French text that's hidden via CSS.
  responses[URL] = base::StringPrintf(
      "<html><body style='display:none'>%s</body></html>", kFrenchText);
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  // Check for no language detected.
  [self assertContentLanguage:@""
             htmlRootLanguage:@""
              adoptedLanguage:base::SysUTF8ToNSString(
                                  translate::kUnknownLanguageCode)];
}

// Tests that language detection is not performed when the page specifies that
// it should not be translated.
- (void)testLanguageDetectionNoTranslate {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  const GURL noTranslateContentURL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPageNoTranslateContent));
  const GURL noTranslateValueURL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPageNoTranslateValue));

  // Load some french page with |content="notranslate"| meta tag.
  [ChromeEarlGrey loadURL:noTranslateContentURL];

  // Check that no language has been detected.
  GREYAssertFalse([self waitForLanguageDetection],
                  @"A language has been detected");

  // Load some french page with |value="notranslate"| meta tag.
  [ChromeEarlGrey loadURL:noTranslateValueURL];

  // Check that no language has been detected.
  GREYAssertFalse([self waitForLanguageDetection],
                  @"A language has been detected");
}

// Tests that history.pushState triggers a new detection.
- (void)testLanguageDetectionWithPushState {
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://scenarioLanguageDetectionPushState");
  std::map<GURL, std::string> responses;
  // Page without meaningful text, language should be undefined ("und").
  responses[URL] = "<html><body>Blahrg :)</body></html>";
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  // Check for no language detected.
  [self assertContentLanguage:@""
             htmlRootLanguage:@""
              adoptedLanguage:base::SysUTF8ToNSString(
                                  translate::kUnknownLanguageCode)];

  // Resets state before triggering a new round of language detection.
  [TranslateAppInterface resetLanguageDetectionTabHelperObserver];
  // Change the text of the page.
  NSError* error = nil;
  [ChromeEarlGreyAppInterface
      executeJavaScript:[NSString stringWithFormat:@"document.write('%s');",
                                                   kEnglishText]
                  error:&error];
  // Trigger a new detection with pushState.
  error = nil;
  [ChromeEarlGreyAppInterface
      executeJavaScript:@"history.pushState(null, null, null);"
                  error:&error];
  // Check that the new language has been detected.
  [self assertContentLanguage:@"" htmlRootLanguage:@"" adoptedLanguage:@"en"];
}

// Tests that language detection is performed on hash changes.
- (void)testLanguageDetectionWithHashChange {
  // Generate a page with French text and a button that changes the text to
  // English and triggers a hash change.
  std::string html = base::StringPrintf(
      "<html><head><script>"
      "function hash() {"
      "  document.getElementById('text').innerHTML = '%s';"
      "  location.href='#1';"
      "}"
      "</script></head><body>"
      "<input type='button' value='Hash' id='Hash' onclick='hash()'>"
      "<div id='text'>%s</div>"
      "</body></html>",
      kEnglishText, kFrenchText);

  // Set up the mock server.
  std::map<GURL, std::string> responses;
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://hashChangeLanguageDetected");
  responses[URL] = html;
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  // Check that language has been detected.
  [self assertContentLanguage:@"" htmlRootLanguage:@"" adoptedLanguage:@"fr"];

  // Resets state before triggering a new round of language detection.
  [TranslateAppInterface resetLanguageDetectionTabHelperObserver];
  // Trigger the hash change.
  [ChromeEarlGrey tapWebStateElementWithID:@"Hash"];
  // Check that language detection has been re-run.
  [self assertContentLanguage:@"" htmlRootLanguage:@"" adoptedLanguage:@"en"];
}

// Tests that language in http content is detected.
- (void)testLanguageDetectionHttpContentLanguage {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // The HTTP header is detected.
  GURL URL = web::test::HttpServer::MakeUrl(std::string("http://") +
                                            kLanguagePath + "?http=fr");
  [ChromeEarlGrey loadURL:URL];
  [self assertContentLanguage:@"fr" htmlRootLanguage:@"" adoptedLanguage:@"fr"];

  // Resets state before triggering a new round of language detection.
  [TranslateAppInterface resetLanguageDetectionTabHelperObserver];
  // Everything after the comma is truncated.
  URL = web::test::HttpServer::MakeUrl(std::string("http://") + kLanguagePath +
                                       "?http=fr,ornot");
  [ChromeEarlGrey loadURL:URL];
  [self assertContentLanguage:@"fr" htmlRootLanguage:@"" adoptedLanguage:@"fr"];

  // Resets state before triggering a new round of language detection.
  [TranslateAppInterface resetLanguageDetectionTabHelperObserver];
  // The HTTP header is overriden by meta tag.
  URL = web::test::HttpServer::MakeUrl(std::string("http://") + kLanguagePath +
                                       "?http=fr&meta=it");
  [ChromeEarlGrey loadURL:URL];
  [self assertContentLanguage:@"it" htmlRootLanguage:@"" adoptedLanguage:@"it"];

  // Resets state before triggering a new round of language detection.
  [TranslateAppInterface resetLanguageDetectionTabHelperObserver];
  // Only the header of the main page is detected.
  URL =
      web::test::HttpServer::MakeUrl(std::string("http://") + kSubresourcePath);
  [ChromeEarlGrey loadURL:URL];
  [self assertContentLanguage:@"fr" htmlRootLanguage:@"" adoptedLanguage:@"fr"];
}

// Tests that language in http content is detected when navigating to a link.
- (void)testLanguageDetectionHttpContentLanguageBehindLink {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Detection works when clicking on a link.
  GURL URL = web::test::HttpServer::MakeUrl(std::string("http://") + kLinkPath);
  GURL someLanguageURL = web::test::HttpServer::MakeUrl(kSomeLanguageUrl);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey tapWebStateElementWithID:@"click"];
  [ChromeEarlGrey waitForWebStateContainingText:kLanguagePathText];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          someLanguageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [self assertContentLanguage:@"es" htmlRootLanguage:@"" adoptedLanguage:@"es"];
}

// Tests that language detection still happens when a very large quantity of
// text is present on the page.
- (void)testLanguageDetectionLargePage {
  // Generate very large page.
  std::string html = "<html lang='fr'><body>";
  NSUInteger targetSize = 1024 * 1024;  // More than 1 MB of page content.
  while (html.length() < targetSize) {
    html.append("<p>");
    html.append(kFrenchText);
    html.append("</p>");
  }
  html.append("</body></html>");

  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://languageDetectionLargePage");
  responses[URL] = html;
  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:URL];

  // Check that language has been detected.
  [self assertContentLanguage:@"" htmlRootLanguage:@"fr" adoptedLanguage:@"fr"];
}

// Tests that language detection is not performed when translate is disabled.
- (void)testLanguageDetectionDisabled {
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://scenarioLanguageDetectionDisabled");
  std::map<GURL, std::string> responses;
  // A page with some text.
  responses[URL] = "<html><body>Hello world!</body></html>";
  web::test::SetUpSimpleHttpServer(responses);

  // Disable translate.
  [ChromeEarlGreyAppInterface
      setBoolValue:NO
       forUserPref:base::SysUTF8ToNSString(prefs::kOfferTranslateEnabled)];

  // Open some webpage.
  [ChromeEarlGrey loadURL:URL];
  // Check that no language has been detected.
  GREYAssertFalse([self waitForLanguageDetection],
                  @"A language has been detected");

  // Enable translate.
  [ChromeEarlGreyAppInterface
      setBoolValue:YES
       forUserPref:base::SysUTF8ToNSString(prefs::kOfferTranslateEnabled)];
}

// Tests that the infobar hides/shows as the browser enters/exits the fullscreen
// mode as well as it can be dimissed.
- (void)testInfobarShowHideDismiss {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // Scroll down to enter the fullscreen mode.
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Expect the translate infobar to be hidden.
  [[EarlGrey selectElementWithMatcher:TranslateInfobar()]
      assertWithMatcher:grey_notVisible()];

  // Scroll up to exit the fullscreen mode.
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  [self assertTranslateInfobarIsVisible];

  // Dismiss the translate infobar.
  [[EarlGrey selectElementWithMatcher:CloseButton()] performAction:grey_tap()];

  // Wait until the translate infobar disappears.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:TranslateInfobar()]
        assertWithMatcher:grey_nil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Translate infobar failed to disappear.");
}

// Tests that the infobar's popup menu can be dimissed.
- (void)testInfobarDismissPopupMenu {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Expect the translate options menu to have appeared.
  [[EarlGrey selectElementWithMatcher:OptionsMenu()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // When the popup menu is visible, a scrim covers the whole window and tapping
  // it dismisses the popup menu. The options button is outside of the bounds of
  // the popup menu and is a convenient place to tap to activate the scrim.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Expect the translate options menu to have disappeared.
  [[EarlGrey selectElementWithMatcher:OptionsMenu()]
      assertWithMatcher:grey_nil()];
}

// Tests that the page can be translated and that translation can be reverted
// using the source and the target language tabs.
- (void)testInfobarTranslateRevert {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self translateThenRevert];
}

// Tests that the page can be translated and that translation can be reverted
// using the source and the target language tabs in incognito mode.
- (void)testInfobarTranslateRevertIncognito {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text in an incognito tab.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  // Stop observing the current IOSLanguageDetectionTabHelper before opening the
  // incognito tab.
  [TranslateAppInterface tearDownLanguageDetectionTabHelperObserver];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL];

  // Since current web state has changed to Incognito, a new fake translation
  // manager has to be set up for the rest of this test.
  [TranslateAppInterface setUpFakeJSTranslateManagerInCurrentTab];

  [self translateThenRevert];
}

// Translates the page and reverts the translation using the language tabs.
- (void)translateThenRevert {
  [self assertTranslateInfobarIsVisible];

  // Make sure the page is not translated.
  [ChromeEarlGrey waitForWebStateNotContainingText:"Translated"];

  // The source language tab must be selected and the target language tab must
  // not. Translate the page by tapping the target language tab.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"French")]
      assertWithMatcher:ElementIsSelected(YES)];
  [[[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
      assertWithMatcher:ElementIsSelected(NO)] performAction:grey_tap()];

  // Make sure the page is translated.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];

  // The target language tab must be selected and the source language tab must
  // not. Revert the translation by tapping the source language tab.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
      assertWithMatcher:ElementIsSelected(YES)];
  [[[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"French")]
      assertWithMatcher:ElementIsSelected(NO)] performAction:grey_tap()];

  // Make sure the translation is reverted.
  [ChromeEarlGrey waitForWebStateNotContainingText:"Translated"];

  // The source language tab must be selected and the target language tab must
  // not.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"French")]
      assertWithMatcher:ElementIsSelected(YES)];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
      assertWithMatcher:ElementIsSelected(NO)];
}

// Tests that translation occurs automatically on second navigation to an
// already translated page.
- (void)testInfobarAutoTranslate {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text and a link.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPageWithLinkPath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // Make sure the page is not translated.
  [ChromeEarlGrey waitForWebStateNotContainingText:"Translated"];

  // The target language tab must not be selected. Translate the page by
  // tapping the target language tab.
  [[[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
      assertWithMatcher:ElementIsSelected(NO)] performAction:grey_tap()];

  // Make sure the page is translated.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];

  // Click on the link.
  [ChromeEarlGrey tapWebStateElementWithID:@"link"];

  // Make sure the navigation is completed.
  GURL frenchPagePathURL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          frenchPagePathURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Make sure the page is automatically translated.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];
}

// Tests that the source and the target languages can be changed.
- (void)testInfobarChangeLanguages {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // The source language tab must be selected and the target language tab must
  // not.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"French")]
      assertWithMatcher:ElementIsSelected(YES)];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
      assertWithMatcher:ElementIsSelected(NO)];

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Expect the translate options menu to have appeared.
  [[EarlGrey selectElementWithMatcher:OptionsMenu()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select the "More Languages" entry in the options popup menu.
  [[EarlGrey selectElementWithMatcher:MoreLanguages()]
      performAction:grey_tap()];

  // Expect the translate options menu to have disappeared.
  [[EarlGrey selectElementWithMatcher:OptionsMenu()]
      assertWithMatcher:grey_nil()];

  // Expect the language selection menu to have appeared.
  [[EarlGrey selectElementWithMatcher:LanguagesMenu()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select "Dutch" from the the popup menu.
  [[[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Dutch")]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:LanguagesMenu()] performAction:grey_tap()];

  // Expect the language selection menu to have disappeared.
  [[EarlGrey selectElementWithMatcher:LanguagesMenu()]
      assertWithMatcher:grey_nil()];

  // Make sure the page is translated.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];

  // Make sure the target language changes to "Dutch". The target language
  // tab must be selected and the source language tab must not. Revert the
  // translation by tapping the source language tab.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Dutch")]
      assertWithMatcher:ElementIsSelected(YES)];
  [[[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"French")]
      assertWithMatcher:ElementIsSelected(NO)] performAction:grey_tap()];

  // Make sure the translation is reverted.
  [ChromeEarlGrey waitForWebStateNotContainingText:"Translated"];

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Select the "Page not in French" entry in the options popup menu.
  [[EarlGrey selectElementWithMatcher:PageNotIn(@"French")]
      performAction:grey_tap()];

  // Expect the translate options menu to have disappeared.
  [[EarlGrey selectElementWithMatcher:OptionsMenu()]
      assertWithMatcher:grey_nil()];

  // Expect the language selection menu to have appeared.
  [[EarlGrey selectElementWithMatcher:LanguagesMenu()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select "English" from the the popup menu.
  [[[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:LanguagesMenu()] performAction:grey_tap()];

  // Make sure the page is translated.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];

  // Make sure the source language changes to "English". The target language
  // tab must be selected and the source language tab must not.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
      assertWithMatcher:ElementIsSelected(NO)];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Dutch")]
      assertWithMatcher:ElementIsSelected(YES)];
}

// Tests that the "Always Translate" options can be toggled and the prefs are
// updated accordingly.
- (void)testInfobarAlwaysTranslate {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // Make sure that French to English translation is not automatic.
  GREYAssert(![TranslateAppInterface shouldAutoTranslateFromLanguage:@"fr"
                                                          toLanguage:@"en"],
             @"French to English translation is automatic");

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Make sure the "Always Translate French" entry is not selected and tap it.
  [[[EarlGrey selectElementWithMatcher:AlwaysTranslate(@"French")]
      assertWithMatcher:ElementIsSelected(NO)] performAction:grey_tap()];

  // Expect the translate options menu to have disappeared.
  [[EarlGrey selectElementWithMatcher:OptionsMenu()]
      assertWithMatcher:grey_nil()];

  // Make sure the page is not translated yet.
  [ChromeEarlGrey waitForWebStateNotContainingText:"Translated"];

  // Make sure that French to English translation is not automatic yet.
  GREYAssert(![TranslateAppInterface shouldAutoTranslateFromLanguage:@"fr"
                                                          toLanguage:@"en"],
             @"French to English translation is automatic");

  // Tap the notification snackbar to dismiss it.
  NSString* snackbarTitle =
      l10n_util::GetNSStringF(IDS_TRANSLATE_NOTIFICATION_ALWAYS_TRANSLATE,
                              base::SysNSStringToUTF16(@"French"),
                              base::SysNSStringToUTF16(@"English"));
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarTitle)]
      performAction:grey_tap()];

  // Make sure the page is translated after the snackbar is dismissed.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];

  // Make sure that French to English translation is automatic after the
  // snackbar is dismissed.
  GREYAssert([TranslateAppInterface shouldAutoTranslateFromLanguage:@"fr"
                                                         toLanguage:@"en"],
             @"French to English translation is not automatic");

  // Reload the page.
  [ChromeEarlGrey reload];

  [self assertTranslateInfobarIsVisible];

  // Make sure the page is translated.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];

  // The target language tab must be selected and the source language tab must
  // not.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"French")]
      assertWithMatcher:ElementIsSelected(NO)];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
      assertWithMatcher:ElementIsSelected(YES)];

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Make sure the "Always Translate French" entry is now selected and tap it.
  [[[EarlGrey selectElementWithMatcher:AlwaysTranslate(@"French")]
      assertWithMatcher:ElementIsSelected(YES)] performAction:grey_tap()];

  // Make sure that French to English translation is no longer automatic.
  GREYAssert(![TranslateAppInterface shouldAutoTranslateFromLanguage:@"fr"
                                                          toLanguage:@"en"],
             @"French to English translation is automatic");

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Make sure the "Always Translate French" entry is not selected and tap it.
  [[[EarlGrey selectElementWithMatcher:AlwaysTranslate(@"French")]
      assertWithMatcher:ElementIsSelected(NO)] performAction:grey_tap()];

  // Tap the notification snackbar's "UNDO" button.
  [[EarlGrey selectElementWithMatcher:UndoButton()] performAction:grey_tap()];

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Make sure the "Always Translate French" entry is still not selected.
  [[EarlGrey selectElementWithMatcher:AlwaysTranslate(@"French")]
      assertWithMatcher:ElementIsSelected(NO)];

  // Make sure that French to English translation is not automatic.
  GREYAssert(![TranslateAppInterface shouldAutoTranslateFromLanguage:@"fr"
                                                          toLanguage:@"en"],
             @"French to English translation is automatic");
}

// Tests that "Always Translate" is automatically triggered after a minimum
// number of translate attempts by the user.
- (void)testInfobarAutoAlwaysTranslate {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // Make sure that French to English translation is not automatic.
  GREYAssert(![TranslateAppInterface shouldAutoTranslateFromLanguage:@"fr"
                                                          toLanguage:@"en"],
             @"French to English translation is automatic");

  // Translate the page by tapping the target language tab until
  // "Always Translate" is automatically triggered.
  for (int i = 0; i <= [TranslateAppInterface infobarAutoAlwaysThreshold];
       i++) {
    [[EarlGrey
        selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"French")]
        performAction:grey_tap()];
  }

  // Make sure that French to English translation is not automatic yet.
  GREYAssert(![TranslateAppInterface shouldAutoTranslateFromLanguage:@"fr"
                                                          toLanguage:@"en"],
             @"French to English translation is automatic");

  // Tap the notification snackbar to dismiss it.
  NSString* snackbarTitle =
      l10n_util::GetNSStringF(IDS_TRANSLATE_NOTIFICATION_ALWAYS_TRANSLATE,
                              base::SysNSStringToUTF16(@"French"),
                              base::SysNSStringToUTF16(@"English"));
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarTitle)]
      performAction:grey_tap()];

  // Make sure that French to English translation is automatic after the
  // snackbar is dismissed.
  GREYAssert([TranslateAppInterface shouldAutoTranslateFromLanguage:@"fr"
                                                         toLanguage:@"en"],
             @"French to English translation is not automatic");
}

// Tests that "Always Translate" is automatically triggered only for a maximum
// number of times if refused by the user.
- (void)testInfobarAutoAlwaysTranslateMaxTries {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // Make sure that French to English translation is not automatic.
  GREYAssert(![TranslateAppInterface shouldAutoTranslateFromLanguage:@"fr"
                                                          toLanguage:@"en"],
             @"French to English translation is automatic");

  // Trigger and refuse the auto "Always Translate".
  for (int i = 0; i < [TranslateAppInterface infobarMaximumNumberOfAutoAlways];
       i++) {
    // Translate the page by tapping the target language tab until
    // "Always Translate" is automatically triggered.
    for (int j = 0; j <= [TranslateAppInterface infobarAutoAlwaysThreshold];
         j++) {
      [[EarlGrey
          selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
          performAction:grey_tap()];
      [[EarlGrey
          selectElementWithMatcher:ButtonWithAccessibilityLabel(@"French")]
          performAction:grey_tap()];
    }
    // Tap the notification snackbar's "UNDO" button.
    [[EarlGrey selectElementWithMatcher:UndoButton()] performAction:grey_tap()];
  }

  // Translate the page by tapping the target language tab in order to
  // automatically trigger "Always Translate".
  for (int i = 0; i <= [TranslateAppInterface infobarAutoAlwaysThreshold];
       i++) {
    [[EarlGrey
        selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
        performAction:grey_tap()];
  }

  // Make sure "Always Translate" is not triggered.
  NSString* snackbarTitle =
      l10n_util::GetNSStringF(IDS_TRANSLATE_NOTIFICATION_ALWAYS_TRANSLATE,
                              base::SysNSStringToUTF16(@"French"),
                              base::SysNSStringToUTF16(@"English"));
  GREYAssertFalse([self waitForElementToAppearOrTimeout:grey_accessibilityLabel(
                                                            snackbarTitle)],
                  @"Always Translate was triggered.");
}

// Tests that the "Never Translate ..." options dismisses the infobar and
// updates the prefs accordingly.
- (void)testInfobarNeverTranslate {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // Make sure that translation from French is not blocked.
  GREYAssert(![TranslateAppInterface isBlockedLanguage:@"fr"],
             @"Translation from French is blocked");

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Tap the "Never Translate French" entry.
  [[EarlGrey selectElementWithMatcher:NeverTranslate(@"French")]
      performAction:grey_tap()];

  // Expect the translate options menu to have disappeared.
  [[EarlGrey selectElementWithMatcher:OptionsMenu()]
      assertWithMatcher:grey_nil()];

  // Tap the notification snackbar's "UNDO" button.
  [[EarlGrey selectElementWithMatcher:UndoButton()] performAction:grey_tap()];

  // Make sure that translation from French is still not blocked.
  GREYAssert(![TranslateAppInterface isBlockedLanguage:@"fr"],
             @"Translation from French is blocked");

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Tap the "Never Translate French" entry.
  [[EarlGrey selectElementWithMatcher:NeverTranslate(@"French")]
      performAction:grey_tap()];

  // Make sure that translation from French is not blocked yet.
  GREYAssert(![TranslateAppInterface isBlockedLanguage:@"fr"],
             @"Translation from French is blocked");

  // Tap the notification snackbar to dismiss it.
  NSString* snackbarTitle =
      l10n_util::GetNSStringF(IDS_TRANSLATE_NOTIFICATION_LANGUAGE_NEVER,
                              base::SysNSStringToUTF16(@"French"));
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarTitle)]
      performAction:grey_tap()];

  // Wait until the translate infobar disappears.
  GREYAssert([self waitForElementToDisappearOrTimeout:TranslateInfobar()],
             @"Translate infobar failed to disappear.");

  // Make sure that translation from French is blocked after the snackbar is
  // dismissed.
  GREYAssert([TranslateAppInterface isBlockedLanguage:@"fr"],
             @"Translation from French is not blocked");

  // Reload the page.
  [ChromeEarlGrey reload];

  // Make sure the translate infobar does not appear.
  GREYAssertFalse([self waitForElementToAppearOrTimeout:TranslateInfobar()],
                  @"Translate infobar appeared.");
}

// Tests that "Never Translate ..." is automatically triggered after a minimum
// number of translate infobar dismissals by the user.
- (void)testInfobarAutoNeverTranslate {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // Make sure that translation from French is not blocked.
  GREYAssert(![TranslateAppInterface isBlockedLanguage:@"fr"],
             @"Translation from French is blocked");

  // Dismiss the translate infobar until "Never Translate ..." is automatically
  // triggered.
  for (int i = 0; i < [TranslateAppInterface infobarAutoNeverThreshold]; i++) {
    // Reload the page.
    [ChromeEarlGrey reload];

    [self assertTranslateInfobarIsVisible];

    // Dismiss the translate infobar.
    [[EarlGrey selectElementWithMatcher:CloseButton()]
        performAction:grey_tap()];
  }

  // Make sure that translation from French is not blocked yet.
  GREYAssert(![TranslateAppInterface isBlockedLanguage:@"fr"],
             @"Translation from French is blocked");

  // Tap the notification snackbar to dismiss it.
  NSString* snackbarTitle =
      l10n_util::GetNSStringF(IDS_TRANSLATE_NOTIFICATION_LANGUAGE_NEVER,
                              base::SysNSStringToUTF16(@"French"));
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarTitle)]
      performAction:grey_tap()];

  // Wait until the translate infobar disappears.
  GREYAssert([self waitForElementToDisappearOrTimeout:TranslateInfobar()],
             @"Translate infobar failed to disappear.");

  // Make sure that translation from French is blocked after the snackbar is
  // dismissed.
  GREYAssert([TranslateAppInterface isBlockedLanguage:@"fr"],
             @"Translation from French is not blocked");
}

// Tests that "Never Translate ..." is automatically triggered only for a
// maximum number of times if refused by the user.
// TODO(crbug.com/945118): Re-enable when fixed.
- (void)DISABLED_testInfobarAutoNeverTranslateMaxTries {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // Make sure that translation from French is not blocked.
  GREYAssert(![TranslateAppInterface isBlockedLanguage:@"fr"],
             @"Translation from French is blocked");

  // Trigger and refuse the auto "Never Translate ...".
  for (int i = 0; i < [TranslateAppInterface infobarMaximumNumberOfAutoNever];
       i++) {
    // Dismiss the translate infobar until "Never Translate ..." is
    // automatically triggered.
    for (int j = 0; j < [TranslateAppInterface infobarAutoNeverThreshold];
         j++) {
      // Reload the page.
      [ChromeEarlGrey reload];

      [self assertTranslateInfobarIsVisible];

      // Dismiss the translate infobar.
      [[EarlGrey selectElementWithMatcher:CloseButton()]
          performAction:grey_tap()];
    }
    // Tap the notification snackbar's "UNDO" button.
    [[EarlGrey selectElementWithMatcher:UndoButton()] performAction:grey_tap()];

    // Wait until the translate infobar disappears.
    GREYAssert([self waitForElementToDisappearOrTimeout:TranslateInfobar()],
               @"Translate infobar failed to disappear.");
  }

  // Dismiss the translate infobar in order to automatically trigger
  // "Never Translate ...".
  for (int i = 0; i < [TranslateAppInterface infobarAutoNeverThreshold]; i++) {
    // Reload the page.
    [ChromeEarlGrey reload];

    [self assertTranslateInfobarIsVisible];

    // Dismiss the translate infobar.
    [[EarlGrey selectElementWithMatcher:CloseButton()]
        performAction:grey_tap()];
  }

  // Make sure "Never Translate ..." is not triggered.
  NSString* snackbarTitle =
      l10n_util::GetNSStringF(IDS_TRANSLATE_NOTIFICATION_LANGUAGE_NEVER,
                              base::SysNSStringToUTF16(@"French"));
  GREYAssertFalse([self waitForElementToAppearOrTimeout:grey_accessibilityLabel(
                                                            snackbarTitle)],
                  @"Never Translate French was triggered.");
}

// Tests that the "Never Translate this site" option dismisses the infobar and
// updates the prefs accordingly.
- (void)testInfobarNeverTranslateSite {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  NSString* URLHost = base::SysUTF8ToNSString(URL.HostNoBrackets());
  // Make sure that translation for the site is not blocked.
  GREYAssert(![TranslateAppInterface isBlockedSite:URLHost],
             @"Translation is blocked for the site");

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Tap the "Never Translate this site" entry.
  [[EarlGrey selectElementWithMatcher:NeverTranslateSite()]
      performAction:grey_tap()];

  // Expect the translate options menu to have disappeared.
  [[EarlGrey selectElementWithMatcher:OptionsMenu()]
      assertWithMatcher:grey_nil()];

  // Tap the notification snackbar's "UNDO" button.
  [[EarlGrey selectElementWithMatcher:UndoButton()] performAction:grey_tap()];

  // Make sure that translation for the site is still not blocked.
  GREYAssert(![TranslateAppInterface isBlockedSite:URLHost],
             @"Translation is blocked for the site");

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Tap the "Never Translate this site" entry.
  [[EarlGrey selectElementWithMatcher:NeverTranslateSite()]
      performAction:grey_tap()];

  // Make sure that translation for the site is not blocked yet.
  GREYAssert(![TranslateAppInterface isBlockedSite:URLHost],
             @"Translation is blocked for the site");

  // Tap the notification snackbar to dismiss it.
  NSString* snackbarTitle =
      l10n_util::GetNSString(IDS_TRANSLATE_NOTIFICATION_SITE_NEVER);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarTitle)]
      performAction:grey_tap()];

  // Make sure that translation for the site is blocked after the snackbar is
  // dismissed.
  GREYAssert([TranslateAppInterface isBlockedSite:URLHost],
             @"Translation is not blocked for the site");

  // Wait until the translate infobar disappears.
  GREYAssert([self waitForElementToDisappearOrTimeout:TranslateInfobar()],
             @"Translate infobar failed to disappear.");

  // Reload the page.
  [ChromeEarlGrey reload];

  // Make sure the translate infobar does not appear.
  GREYAssertFalse([self waitForElementToAppearOrTimeout:TranslateInfobar()],
                  @"Translate infobar appeared.");
}

// Tests that the "Translate..." button in the tools menu is enabled if
// translate is available and it brings up the Translate infobar and translates
// the page when tapped. If the page is already translated the infobar should
// appear in "after translate" state.
- (void)testTranslateManualTrigger {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // The source language tab must be selected and the target language tab must
  // not.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"French")]
      assertWithMatcher:ElementIsSelected(YES)];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
      assertWithMatcher:ElementIsSelected(NO)];

  // Dismiss the translate infobar.
  [[EarlGrey selectElementWithMatcher:CloseButton()] performAction:grey_tap()];

  // Wait until the translate infobar disappears.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:TranslateInfobar()]
        assertWithMatcher:grey_nil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Translate infobar failed to disappear.");

  // Make sure the Translate manual trigger button is enabled and tap it.
  [ChromeEarlGreyUI openToolsMenu];
  [[[[EarlGrey selectElementWithMatcher:toolsMenuTranslateButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:ToolsMenuView()]
      assertWithMatcher:grey_not(grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled))]
      performAction:grey_tap()];

  // Make sure the infobar reappears.
  [self assertTranslateInfobarIsVisible];

  // The target language tab must be selected and the source language tab must
  // not.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
      assertWithMatcher:ElementIsSelected(YES)];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"French")]
      assertWithMatcher:ElementIsSelected(NO)];

  // Make sure the page is translated.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];

  // Dismiss the translate infobar.
  [[EarlGrey selectElementWithMatcher:CloseButton()] performAction:grey_tap()];

  // Make sure the Translate manual trigger button is enabled and tap it.
  [ChromeEarlGreyUI openToolsMenu];
  [[[[EarlGrey selectElementWithMatcher:toolsMenuTranslateButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:ToolsMenuView()]
      assertWithMatcher:grey_not(grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled))]
      performAction:grey_tap()];

  // Make sure the infobar reappears.
  [self assertTranslateInfobarIsVisible];

  // The target language tab must be selected and the source language tab must
  // not.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
      assertWithMatcher:ElementIsSelected(YES)];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"French")]
      assertWithMatcher:ElementIsSelected(NO)];
}

// Tests that the "Translate..." button in the tools menu brings up the
// Translate infobar even if user has previously selected not to translate the
// the source language.
- (void)testTranslateManualTriggerNeverTranslate {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Tap the "Never Translate French" entry.
  [[EarlGrey selectElementWithMatcher:NeverTranslate(@"French")]
      performAction:grey_tap()];

  // Tap the notification snackbar to dismiss it.
  NSString* snackbarTitle =
      l10n_util::GetNSStringF(IDS_TRANSLATE_NOTIFICATION_LANGUAGE_NEVER,
                              base::SysNSStringToUTF16(@"French"));
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarTitle)]
      performAction:grey_tap()];

  // Wait until the translate infobar disappears.
  GREYAssert([self waitForElementToDisappearOrTimeout:TranslateInfobar()],
             @"Translate infobar failed to disappear.");

  // Reload the page.
  [ChromeEarlGrey reload];

  // Make sure the translate infobar does not appear.
  GREYAssertFalse([self waitForElementToAppearOrTimeout:TranslateInfobar()],
                  @"Translate infobar appeared.");

  // Tap the Translate manual trigger button.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey selectElementWithMatcher:toolsMenuTranslateButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:ToolsMenuView()] performAction:grey_tap()];

  // Make sure the infobar reappears.
  [self assertTranslateInfobarIsVisible];
}

// Tests that the "Translate..." button in the tools menu brings up the
// Translate infobar even if user has previously selected not to translate the
// the site.
- (void)testTranslateManualTriggerNeverTranslateSite {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  [self assertTranslateInfobarIsVisible];

  // Open the translate options menu.
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      performAction:grey_tap()];

  // Tap the "Never Translate this site" entry.
  [[EarlGrey selectElementWithMatcher:NeverTranslateSite()]
      performAction:grey_tap()];

  // Tap the notification snackbar to dismiss it.
  NSString* snackbarTitle =
      l10n_util::GetNSString(IDS_TRANSLATE_NOTIFICATION_SITE_NEVER);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarTitle)]
      performAction:grey_tap()];

  // Wait until the translate infobar disappears.
  GREYAssert([self waitForElementToDisappearOrTimeout:TranslateInfobar()],
             @"Translate infobar failed to disappear.");

  // Reload the page.
  [ChromeEarlGrey reload];

  // Make sure the translate infobar does not appear.
  GREYAssertFalse([self waitForElementToAppearOrTimeout:TranslateInfobar()],
                  @"Translate infobar appeared.");

  // Tap the Translate manual trigger button.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey selectElementWithMatcher:toolsMenuTranslateButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:ToolsMenuView()] performAction:grey_tap()];

  // Make sure the infobar reappears.
  [self assertTranslateInfobarIsVisible];
}

// Tests that the "Translate..." button in the tools menu is disabled if
// translate is not available.
- (void)testTranslateManualTriggerNotEnabled {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text with |content="notranslate"| meta tag.
  GURL noTranslateContentURL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPageNoTranslateContent));
  [ChromeEarlGrey loadURL:noTranslateContentURL];

  // Make sure no language has been detected.
  GREYAssertFalse([self waitForLanguageDetection],
                  @"A language has been detected");

  // Make sure the Translate manual trigger button is not enabled.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey selectElementWithMatcher:toolsMenuTranslateButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:ToolsMenuView()]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
  // Close the tools menu.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Load a page with French text with |value="notranslate"| meta tag.
  GURL noTranslateValueURL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPageNoTranslateValue));
  [ChromeEarlGrey loadURL:noTranslateValueURL];

  // Make sure no language has been detected.
  GREYAssertFalse([self waitForLanguageDetection],
                  @"A language has been detected");

  // Make sure the Translate manual trigger button is not enabled.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey selectElementWithMatcher:toolsMenuTranslateButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:ToolsMenuView()]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
  // Close the tools menu.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Load a chrome:// page.
  GURL URL = web::test::HttpServer::MakeUrl("chrome://something-internal");
  [ChromeEarlGrey loadURL:URL];

  // Make sure the Translate manual trigger button is not enabled.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey selectElementWithMatcher:toolsMenuTranslateButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:ToolsMenuView()]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
  // Close the tools menu.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
}

#pragma mark - Utility methods

- (void)assertTranslateInfobarIsVisible {
  // Wait until the translate infobar becomes visible.
  GREYAssert([self waitForElementToAppearOrTimeout:TranslateInfobar()],
             @"Translate infobar failed to show.");

  // Check that the translate infobar is fully visible.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"French")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:OptionsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (BOOL)waitForElementToAppearOrTimeout:(id<GREYMatcher>)matcher {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()
                                                             error:&error];
    return error == nil;
  };
  return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition);
}

- (BOOL)waitForElementToDisappearOrTimeout:(id<GREYMatcher>)matcher {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_nil()
                                                             error:&error];
    return error == nil;
  };
  return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition);
}

// Returns whether a language has been detected on the current page. Returns
// false if a timeout was detected while waiting for language detection.
- (BOOL)waitForLanguageDetection {
  bool detected = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return [TranslateAppInterface isLanguageDetected];
  });
  return detected;
}

// Waits until language details have been detected then verifies them.
// Checks expectation for Content-Language, HTML root element language, and
// detected language on page. Use @"" for expected values that are expected
// to be not set.
- (void)assertContentLanguage:(NSString*)expectedContentLanguage
             htmlRootLanguage:(NSString*)expectedHtmlRootLanguage
              adoptedLanguage:(NSString*)expectedAdoptedLanguage {
  GREYAssert([self waitForLanguageDetection], @"Language not detected");

  NSString* contentLanguage = [TranslateAppInterface contentLanguage];
  NSString* contentLanguageError =
      [NSString stringWithFormat:@"Wrong content-language: %@ (expected %@)",
                                 contentLanguage, expectedContentLanguage];
  GREYAssertEqualObjects(expectedContentLanguage, contentLanguage,
                         contentLanguageError);

  NSString* htmlRootLanguage = [TranslateAppInterface htmlRootLanguage];
  NSString* htmlRootLanguageError =
      [NSString stringWithFormat:@"Wrong html root language: %@ (expected %@)",
                                 htmlRootLanguage, expectedHtmlRootLanguage];
  GREYAssertEqualObjects(expectedHtmlRootLanguage, htmlRootLanguage,
                         htmlRootLanguageError);

  NSString* adoptedLanguage = [TranslateAppInterface adoptedLanguage];
  NSString* adoptedLanguageError =
      [NSString stringWithFormat:@"Wrong adopted language: %@ (expected %@)",
                                 adoptedLanguage, expectedAdoptedLanguage];
  GREYAssertEqualObjects(expectedAdoptedLanguage, adoptedLanguage,
                         adoptedLanguageError);
}

@end
