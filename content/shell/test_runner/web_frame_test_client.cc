// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_frame_test_client.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_frame.h"
#include "content/shell/common/web_test/web_test_string_util.h"
#include "content/shell/renderer/web_test/blink_test_helpers.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/test_runner/accessibility_controller.h"
#include "content/shell/test_runner/event_sender.h"
#include "content/shell/test_runner/gc_controller.h"
#include "content/shell/test_runner/mock_screen_orientation_client.h"
#include "content/shell/test_runner/spell_check_client.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_plugin.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/web_frame_test_proxy.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Used to write a platform neutral file:/// URL by taking the
// filename and its directory. (e.g., converts
// "file:///tmp/foo/bar.txt" to just "bar.txt").
std::string DescriptionSuitableForTestResult(const std::string& url) {
  if (url.empty() || std::string::npos == url.find("file://"))
    return url;

  size_t pos = url.rfind('/');
  if (pos == std::string::npos || !pos)
    return "ERROR:" + url;
  pos = url.rfind('/', pos - 1);
  if (pos == std::string::npos)
    return "ERROR:" + url;

  return url.substr(pos + 1);
}

void BlockRequest(blink::WebURLRequest& request) {
  request.SetUrl(GURL("255.255.255.255"));
}

bool IsLocalHost(const std::string& host) {
  return host == "127.0.0.1" || host == "localhost" || host == "[::1]";
}

bool IsTestHost(const std::string& host) {
  return base::EndsWith(host, ".test", base::CompareCase::INSENSITIVE_ASCII) ||
         base::EndsWith(host, ".test.", base::CompareCase::INSENSITIVE_ASCII);
}

bool HostIsUsedBySomeTestsToGenerateError(const std::string& host) {
  return host == "255.255.255.255";
}

// Used to write a platform neutral file:/// URL by only taking the filename
// (e.g., converts "file:///tmp/foo.txt" to just "foo.txt").
std::string URLSuitableForTestResult(const std::string& url) {
  if (url.empty() || std::string::npos == url.find("file://"))
    return url;

  size_t pos = url.rfind('/');
  if (pos == std::string::npos) {
#ifdef WIN32
    pos = url.rfind('\\');
    if (pos == std::string::npos)
      pos = 0;
#else
    pos = 0;
#endif
  }
  std::string filename = url.substr(pos + 1);
  if (filename.empty())
    return "file:";  // A WebKit test has this in its expected output.
  return filename;
}

// WebNavigationType debugging strings taken from PolicyDelegate.mm.
const char* kLinkClickedString = "link clicked";
const char* kFormSubmittedString = "form submitted";
const char* kBackForwardString = "back/forward";
const char* kReloadString = "reload";
const char* kFormResubmittedString = "form resubmitted";
const char* kOtherString = "other";

// Get a debugging string from a WebNavigationType.
const char* WebNavigationTypeToString(blink::WebNavigationType type) {
  switch (type) {
    case blink::kWebNavigationTypeLinkClicked:
      return kLinkClickedString;
    case blink::kWebNavigationTypeFormSubmitted:
      return kFormSubmittedString;
    case blink::kWebNavigationTypeBackForward:
      return kBackForwardString;
    case blink::kWebNavigationTypeReload:
      return kReloadString;
    case blink::kWebNavigationTypeFormResubmitted:
      return kFormResubmittedString;
    case blink::kWebNavigationTypeOther:
      return kOtherString;
  }
  return web_test_string_util::kIllegalString;
}

}  // namespace

WebFrameTestClient::WebFrameTestClient(WebViewTestProxy* web_view_test_proxy,
                                       WebFrameTestProxy* web_frame_test_proxy)
    : web_view_test_proxy_(web_view_test_proxy),
      web_frame_test_proxy_(web_frame_test_proxy) {}

WebFrameTestClient::~WebFrameTestClient() = default;

void WebFrameTestClient::Initialize() {
  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();
  TestRunner* test_runner = interfaces->GetTestRunner();
  blink::WebLocalFrame* frame = web_frame_test_proxy_->GetWebFrame();

  spell_check_ = std::make_unique<SpellCheckClient>(frame);
  frame->SetTextCheckClient(spell_check_.get());

  frame->SetContentSettingsClient(test_runner->GetWebContentSettings());
}

void WebFrameTestClient::Reset() {
  spell_check_->Reset();
}

// static
std::string WebFrameTestClient::GetFrameDescription(
    blink::WebLocalFrame* frame) {
  auto* frame_proxy = static_cast<WebFrameTestProxy*>(frame->Client());
  std::string name = frame_proxy->GetFrameNameForWebTests();
  if (frame == frame->View()->MainFrame()) {
    DCHECK(name.empty());
    return "main frame";
  }
  if (name.empty()) {
    return "frame (anonymous)";
  }
  return std::string("frame \"") + name + "\"";
}

void WebFrameTestClient::PostAccessibilityEvent(const ui::AXEvent& event) {
  const char* event_name = nullptr;
  switch (event.event_type) {
    case ax::mojom::Event::kActiveDescendantChanged:
      event_name = "ActiveDescendantChanged";
      break;
    case ax::mojom::Event::kAriaAttributeChanged:
      event_name = "AriaAttributeChanged";
      break;
    case ax::mojom::Event::kAutocorrectionOccured:
      event_name = "AutocorrectionOccured";
      break;
    case ax::mojom::Event::kBlur:
      event_name = "Blur";
      break;
    case ax::mojom::Event::kCheckedStateChanged:
      event_name = "CheckedStateChanged";
      break;
    case ax::mojom::Event::kChildrenChanged:
      event_name = "ChildrenChanged";
      break;
    case ax::mojom::Event::kClicked:
      event_name = "Clicked";
      break;
    case ax::mojom::Event::kDocumentSelectionChanged:
      event_name = "DocumentSelectionChanged";
      break;
    case ax::mojom::Event::kDocumentTitleChanged:
      event_name = "DocumentTitleChanged";
      break;
    case ax::mojom::Event::kFocus:
      event_name = "Focus";
      break;
    case ax::mojom::Event::kHover:
      event_name = "Hover";
      break;
    case ax::mojom::Event::kInvalidStatusChanged:
      event_name = "InvalidStatusChanged";
      break;
    case ax::mojom::Event::kLayoutComplete:
      event_name = "LayoutComplete";
      break;
    case ax::mojom::Event::kLiveRegionChanged:
      event_name = "LiveRegionChanged";
      break;
    case ax::mojom::Event::kLoadComplete:
      event_name = "LoadComplete";
      break;
    case ax::mojom::Event::kLocationChanged:
      event_name = "LocationChanged";
      break;
    case ax::mojom::Event::kMenuListItemSelected:
      event_name = "MenuListItemSelected";
      break;
    case ax::mojom::Event::kMenuListValueChanged:
      event_name = "MenuListValueChanged";
      break;
    case ax::mojom::Event::kRowCollapsed:
      event_name = "RowCollapsed";
      break;
    case ax::mojom::Event::kRowCountChanged:
      event_name = "RowCountChanged";
      break;
    case ax::mojom::Event::kRowExpanded:
      event_name = "RowExpanded";
      break;
    case ax::mojom::Event::kScrollPositionChanged:
      event_name = "ScrollPositionChanged";
      break;
    case ax::mojom::Event::kScrolledToAnchor:
      event_name = "ScrolledToAnchor";
      break;
    case ax::mojom::Event::kSelectedChildrenChanged:
      event_name = "SelectedChildrenChanged";
      break;
    case ax::mojom::Event::kTextSelectionChanged:
      event_name = "SelectedTextChanged";
      break;
    case ax::mojom::Event::kTextChanged:
      event_name = "TextChanged";
      break;
    case ax::mojom::Event::kValueChanged:
      event_name = "ValueChanged";
      break;
    default:
      event_name = "Unknown";
      break;
  }

  blink::WebDocument document =
      web_frame_test_proxy_->GetWebFrame()->GetDocument();
  auto object = blink::WebAXObject::FromWebDocumentByID(document, event.id);
  HandleWebAccessibilityEvent(object, event_name);
}

void WebFrameTestClient::MarkWebAXObjectDirty(const blink::WebAXObject& obj,
                                              bool subtree) {
  HandleWebAccessibilityEvent(obj, "MarkDirty");
}

void WebFrameTestClient::HandleWebAccessibilityEvent(
    const blink::WebAXObject& obj,
    const char* event_name) {
  // Only hook the accessibility events that occurred during the test run.
  // This check prevents false positives in BlinkLeakDetector.
  // The pending tasks in browser/renderer message queue may trigger
  // accessibility events,
  // and AccessibilityController will hold on to their target nodes if we don't
  // ignore them here.
  if (!test_runner()->TestIsRunning())
    return;

  AccessibilityController* accessibility_controller =
      web_view_test_proxy_->accessibility_controller();

  accessibility_controller->NotificationReceived(
      web_frame_test_proxy_->GetWebFrame(), obj, event_name);

  if (accessibility_controller->ShouldLogAccessibilityEvents()) {
    std::string message("AccessibilityNotification - ");
    message += event_name;

    blink::WebNode node = obj.GetNode();
    if (!node.IsNull() && node.IsElementNode()) {
      blink::WebElement element = node.To<blink::WebElement>();
      if (element.HasAttribute("id")) {
        message += " - id:";
        message += element.GetAttribute("id").Utf8().data();
      }
    }

    blink_test_runner()->PrintMessage(message + "\n");
  }
}

void WebFrameTestClient::DidChangeSelection(bool is_empty_callback) {
  if (test_runner()->ShouldDumpEditingCallbacks())
    blink_test_runner()->PrintMessage(
        "EDITING DELEGATE: "
        "webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
}

void WebFrameTestClient::DidChangeContents() {
  if (test_runner()->ShouldDumpEditingCallbacks())
    blink_test_runner()->PrintMessage(
        "EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n");
}

blink::WebPlugin* WebFrameTestClient::CreatePlugin(
    const blink::WebPluginParams& params) {
  blink::WebLocalFrame* frame = web_frame_test_proxy_->GetWebFrame();
  if (TestPlugin::IsSupportedMimeType(params.mime_type)) {
    return TestPlugin::Create(params, web_view_test_proxy_->blink_test_runner(),
                              frame);
  }
  return blink_test_runner()->CreatePluginPlaceholder(params);
}

void WebFrameTestClient::ShowContextMenu(
    const blink::WebContextMenuData& context_menu_data) {
  web_frame_test_proxy_->GetLocalRootWebWidgetTestProxy()
      ->event_sender()
      ->SetContextMenuData(context_menu_data);
}

void WebFrameTestClient::DidStartLoading() {
  test_runner()->AddLoadingFrame(web_frame_test_proxy_->GetWebFrame());
}

void WebFrameTestClient::DidStopLoading() {
  test_runner()->RemoveLoadingFrame(web_frame_test_proxy_->GetWebFrame());
}

void WebFrameTestClient::DidDispatchPingLoader(const blink::WebURL& url) {
  if (test_runner()->ShouldDumpPingLoaderCallbacks())
    blink_test_runner()->PrintMessage(
        std::string("PingLoader dispatched to '") +
        web_test_string_util::URLDescription(url).c_str() + "'.\n");
}

void WebFrameTestClient::WillSendRequest(blink::WebURLRequest& request) {
  // Need to use GURL for host() and SchemeIs()
  GURL url = request.Url();

  // Warning: this may be null in some cross-site cases.
  net::SiteForCookies site_for_cookies = request.SiteForCookies();

  if (test_runner()->HttpHeadersToClear()) {
    for (const std::string& header : *test_runner()->HttpHeadersToClear()) {
      DCHECK(!base::EqualsCaseInsensitiveASCII(header, "referer"));
      request.ClearHttpHeaderField(blink::WebString::FromUTF8(header));
    }
  }

  if (test_runner()->ClearReferrer()) {
    request.SetReferrerString(blink::WebString());
    request.SetReferrerPolicy(
        content::Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
            content::Referrer::GetDefaultReferrerPolicy()));
  }

  std::string host = url.host();
  if (!host.empty() &&
      (url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))) {
    if (!IsLocalHost(host) && !IsTestHost(host) &&
        !HostIsUsedBySomeTestsToGenerateError(host) &&
        ((site_for_cookies.scheme() != url::kHttpScheme &&
          site_for_cookies.scheme() != url::kHttpsScheme) ||
         IsLocalHost(site_for_cookies.registrable_domain())) &&
        !blink_test_runner()->AllowExternalPages()) {
      blink_test_runner()->PrintMessage(
          std::string("Blocked access to external URL ") +
          url.possibly_invalid_spec() + "\n");
      BlockRequest(request);
      return;
    }
  }

  // Set the new substituted URL.
  request.SetUrl(RewriteWebTestsURL(request.Url().GetString().Utf8(),
                                    test_runner()->IsWebPlatformTestsMode()));
}

void WebFrameTestClient::DidAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
  if (!test_runner()->ShouldDumpConsoleMessages())
    return;
  std::string level;
  switch (message.level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      level = "DEBUG";
      break;
    case blink::mojom::ConsoleMessageLevel::kInfo:
      level = "MESSAGE";
      break;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      level = "WARNING";
      break;
    case blink::mojom::ConsoleMessageLevel::kError:
      level = "ERROR";
      break;
    default:
      level = "MESSAGE";
  }
  std::string console_message(std::string("CONSOLE ") + level + ": ");
  // Do not print line numbers if there is no associated source file name.
  // TODO(crbug.com/896194): Figure out why the source line is flaky for empty
  // source names.
  if (!source_name.IsEmpty() && source_line) {
    console_message += base::StringPrintf("line %d: ", source_line);
  }
  // Console messages shouldn't be included in the expected output for
  // web-platform-tests because they may create non-determinism not
  // intended by the test author. They are still included in the stderr
  // output for debug purposes.
  bool dump_to_stderr = test_runner()->IsWebPlatformTestsMode();
  if (!message.text.IsEmpty()) {
    std::string new_message;
    new_message = message.text.Utf8();
    size_t file_protocol = new_message.find("file://");
    if (file_protocol != std::string::npos) {
      new_message = new_message.substr(0, file_protocol) +
                    URLSuitableForTestResult(new_message.substr(file_protocol));
    }
    console_message += new_message;
  }
  console_message += "\n";

  if (dump_to_stderr) {
    blink_test_runner()->PrintMessageToStderr(console_message);
  } else {
    blink_test_runner()->PrintMessage(console_message);
  }
}

bool WebFrameTestClient::ShouldContinueNavigation(
    blink::WebNavigationInfo* info) {
  if (test_runner()->ShouldDumpNavigationPolicy()) {
    blink_test_runner()->PrintMessage(
        "Default policy for navigation to '" +
        web_test_string_util::URLDescription(info->url_request.Url()) +
        "' is '" +
        web_test_string_util::WebNavigationPolicyToString(
            info->navigation_policy) +
        "'\n");
  }

  if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
    GURL url = info->url_request.Url();
    std::string description = WebFrameTestClient::GetFrameDescription(
        web_frame_test_proxy_->GetWebFrame());
    blink_test_runner()->PrintMessage(description +
                                      " - BeginNavigation request to '");
    blink_test_runner()->PrintMessage(
        DescriptionSuitableForTestResult(url.possibly_invalid_spec()));
    blink_test_runner()->PrintMessage("', http method ");
    blink_test_runner()->PrintMessage(
        info->url_request.HttpMethod().Utf8().data());
    blink_test_runner()->PrintMessage("\n");
  }

  bool should_continue = true;
  if (test_runner()->PolicyDelegateEnabled()) {
    blink_test_runner()->PrintMessage(
        std::string("Policy delegate: attempt to load ") +
        web_test_string_util::URLDescription(info->url_request.Url()) +
        " with navigation type '" +
        WebNavigationTypeToString(info->navigation_type) + "'\n");
    should_continue = test_runner()->PolicyDelegateIsPermissive();
    if (test_runner()->PolicyDelegateShouldNotifyDone()) {
      test_runner()->PolicyDelegateDone();
      should_continue = false;
    }
  }

  if (test_runner()->HttpHeadersToClear()) {
    for (const std::string& header : *test_runner()->HttpHeadersToClear()) {
      DCHECK(!base::EqualsCaseInsensitiveASCII(header, "referer"));
      info->url_request.ClearHttpHeaderField(
          blink::WebString::FromUTF8(header));
    }
  }

  if (test_runner()->ClearReferrer()) {
    info->url_request.SetReferrerString(blink::WebString());
    info->url_request.SetReferrerPolicy(
        network::mojom::ReferrerPolicy::kDefault);
  }

  info->url_request.SetUrl(
      RewriteWebTestsURL(info->url_request.Url().GetString().Utf8(),
                         test_runner()->IsWebPlatformTestsMode()));
  return should_continue;
}

void WebFrameTestClient::CheckIfAudioSinkExistsAndIsAuthorized(
    const blink::WebString& sink_id,
    blink::WebSetSinkIdCompleteCallback completion_callback) {
  std::string device_id = sink_id.Utf8();
  if (device_id == "valid" || device_id.empty())
    std::move(completion_callback).Run(/*error =*/base::nullopt);
  else if (device_id == "unauthorized")
    std::move(completion_callback)
        .Run(blink::WebSetSinkIdError::kNotAuthorized);
  else
    std::move(completion_callback).Run(blink::WebSetSinkIdError::kNotFound);
}

void WebFrameTestClient::DidClearWindowObject() {
  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();
  TestRunner* test_runner = interfaces->GetTestRunner();
  WebWidgetTestProxy* web_widget_test_proxy =
      web_frame_test_proxy_->GetLocalRootWebWidgetTestProxy();
  blink::WebLocalFrame* frame = web_frame_test_proxy_->GetWebFrame();

  // These calls will install the various JS bindings for web tests into the
  // frame before JS has a chance to run.
  GCController::Install(frame);
  interfaces->Install(frame);
  test_runner->Install(web_frame_test_proxy_, spell_check_.get(),
                       web_view_test_proxy_->view_test_runner());
  web_view_test_proxy_->Install(frame);
  web_widget_test_proxy->Install(frame);
}

blink::WebEffectiveConnectionType
WebFrameTestClient::GetEffectiveConnectionType() {
  return test_runner()->effective_connection_type();
}

BlinkTestRunner* WebFrameTestClient::blink_test_runner() {
  // TODO(danakj): Return the web_view_test_proxy_'s BlinkTestRunner.
  return web_view_test_proxy_->test_interfaces()->GetFirstBlinkTestRunner();
}

TestRunner* WebFrameTestClient::test_runner() {
  return web_view_test_proxy_->test_interfaces()->GetTestRunner();
}

}  // namespace content
