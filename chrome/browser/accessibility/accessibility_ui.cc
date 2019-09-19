// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/accessibility_tree_formatter.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/escape.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/base/webui/web_ui_util.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#endif

static const char kTargetsDataFile[] = "targets-data.json";

static const char kAccessibilityModeField[] = "a11yMode";
static const char kBrowsersField[] = "browsers";
static const char kEnabledField[] = "enabled";
static const char kErrorField[] = "error";
static const char kFaviconUrlField[] = "faviconUrl";
static const char kFlagNameField[] = "flagName";
static const char kImprovementsEnabledField[] = "improvementsEnabled";
static const char kModeIdField[] = "modeId";
static const char kNameField[] = "name";
static const char kPagesField[] = "pages";
static const char kPidField[] = "pid";
static const char kProcessIdField[] = "processId";
static const char kRequestTypeField[] = "requestType";
static const char kRouteIdField[] = "routeId";
static const char kSessionIdField[] = "sessionId";
static const char kShouldRequestTreeField[] = "shouldRequestTree";
static const char kTreeField[] = "tree";
static const char kTypeField[] = "type";
static const char kUrlField[] = "url";

// Global flags
static const char kBrowser[] = "browser";
static const char kCopyTree[] = "copyTree";
static const char kHTML[] = "html";
static const char kInternal[] = "internal";
static const char kLabelImages[] = "labelImages";
static const char kNative[] = "native";
static const char kPage[] = "page";
static const char kScreenReader[] = "screenreader";
static const char kShowTree[] = "showTree";
static const char kText[] = "text";
static const char kWeb[] = "web";

// Possible global flag values
static const char kDisabled[] = "disabled";
static const char kOff[] = "off";
static const char kOn[] = "on";

namespace {

std::unique_ptr<base::DictionaryValue> BuildTargetDescriptor(
    const GURL& url,
    const std::string& name,
    const GURL& favicon_url,
    int process_id,
    int route_id,
    ui::AXMode accessibility_mode,
    base::ProcessHandle handle = base::kNullProcessHandle) {
  std::unique_ptr<base::DictionaryValue> target_data(
      new base::DictionaryValue());
  target_data->SetInteger(kProcessIdField, process_id);
  target_data->SetInteger(kRouteIdField, route_id);
  target_data->SetString(kUrlField, url.spec());
  target_data->SetString(kNameField, net::EscapeForHTML(name));
  target_data->SetInteger(kPidField, base::GetProcId(handle));
  target_data->SetString(kFaviconUrlField, favicon_url.spec());
  target_data->SetInteger(kAccessibilityModeField, accessibility_mode.mode());
  target_data->SetString(kTypeField, kPage);
  return target_data;
}

std::unique_ptr<base::DictionaryValue> BuildTargetDescriptor(
    content::RenderViewHost* rvh) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  ui::AXMode accessibility_mode;

  std::string title;
  GURL url;
  GURL favicon_url;
  if (web_contents) {
    // TODO(nasko): Fix the following code to use a consistent set of data
    // across the URL, title, and favicon.
    url = web_contents->GetURL();
    title = base::UTF16ToUTF8(web_contents->GetTitle());
    content::NavigationController& controller = web_contents->GetController();
    content::NavigationEntry* entry = controller.GetVisibleEntry();
    if (entry != nullptr && entry->GetURL().is_valid()) {
      gfx::Image favicon_image = entry->GetFavicon().image;
      if (!favicon_image.IsEmpty()) {
        const SkBitmap* favicon_bitmap = favicon_image.ToSkBitmap();
        favicon_url = GURL(webui::GetBitmapDataUrl(*favicon_bitmap));
      }
    }
    accessibility_mode = web_contents->GetAccessibilityMode();
  }

  return BuildTargetDescriptor(url, title, favicon_url,
                               rvh->GetProcess()->GetID(), rvh->GetRoutingID(),
                               accessibility_mode);
}

#if !defined(OS_ANDROID)
std::unique_ptr<base::DictionaryValue> BuildTargetDescriptor(Browser* browser) {
  std::unique_ptr<base::DictionaryValue> target_data(
      new base::DictionaryValue());
  target_data->SetInteger(kSessionIdField, browser->session_id().id());
  target_data->SetString(kNameField,
                         browser->GetWindowTitleForCurrentTab(false));
  target_data->SetString(kTypeField, kBrowser);
  return target_data;
}
#endif  // !defined(OS_ANDROID)

bool ShouldHandleAccessibilityRequestCallback(const std::string& path) {
  return path == kTargetsDataFile;
}

void HandleAccessibilityRequestCallback(
    content::BrowserContext* current_context,
    bool improvements_enabled,
    const std::string& path,
    const content::WebUIDataSource::GotDataCallback& callback) {
  DCHECK(ShouldHandleAccessibilityRequestCallback(path));

  base::DictionaryValue data;
  PrefService* pref = Profile::FromBrowserContext(current_context)->GetPrefs();
  ui::AXMode mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  bool is_native_enabled = content::BrowserAccessibilityState::GetInstance()
                               ->IsRendererAccessibilityEnabled();
  bool native = mode.has_mode(ui::AXMode::kNativeAPIs);
  bool web = mode.has_mode(ui::AXMode::kWebContents);
  bool text = mode.has_mode(ui::AXMode::kInlineTextBoxes);
  bool screenreader = mode.has_mode(ui::AXMode::kScreenReader);
  bool html = mode.has_mode(ui::AXMode::kHTML);

  // The "native" and "web" flags are disabled if
  // --disable-renderer-accessibility is set.
  data.SetString(kNative,
                 is_native_enabled ? (native ? kOn : kOff) : kDisabled);
  data.SetString(kWeb, is_native_enabled ? (web ? kOn : kOff) : kDisabled);

  // The "text", "screenreader" and "html" flags are only
  // meaningful if "web" is enabled.
  bool is_web_enabled = is_native_enabled && web;
  data.SetString(kText, is_web_enabled ? (text ? kOn : kOff) : kDisabled);
  data.SetString(kScreenReader,
                 is_web_enabled ? (screenreader ? kOn : kOff) : kDisabled);
  data.SetString(kHTML, is_web_enabled ? (html ? kOn : kOff) : kDisabled);

  // The "labelImages" flag works only if "web" is enabled, the current profile
  // has the kAccessibilityImageLabelsEnabled preference set and the appropriate
  // command line switch has been used.
  bool are_accessibility_image_labels_enabled =
      is_web_enabled &&
      pref->GetBoolean(prefs::kAccessibilityImageLabelsEnabled);
  bool label_images = mode.has_mode(ui::AXMode::kLabelImages);
  data.SetString(kLabelImages, are_accessibility_image_labels_enabled
                                   ? (label_images ? kOn : kOff)
                                   : kDisabled);

  bool show_internal = pref->GetBoolean(prefs::kShowInternalAccessibilityTree);
  data.SetString(kInternal, show_internal ? kOn : kOff);

  std::unique_ptr<base::ListValue> rvh_list(new base::ListValue());
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());

  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    // Ignore processes that don't have a connection, such as crashed tabs.
    if (!widget->GetProcess()->IsInitializedAndNotDead())
      continue;
    content::RenderViewHost* rvh = content::RenderViewHost::From(widget);
    if (!rvh)
      continue;
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(rvh);
    content::WebContentsDelegate* delegate = web_contents->GetDelegate();
    if (!delegate)
      continue;
    // Ignore views that are never visible, like background pages.
    if (delegate->IsNeverVisible(web_contents))
      continue;
    content::BrowserContext* context = rvh->GetProcess()->GetBrowserContext();
    if (context != current_context)
      continue;

    std::unique_ptr<base::DictionaryValue> descriptor =
        BuildTargetDescriptor(rvh);
    descriptor->SetBoolean(kNative, is_native_enabled);
    descriptor->SetBoolean(kWeb, is_web_enabled);
    descriptor->SetBoolean(kLabelImages,
                           are_accessibility_image_labels_enabled);
    rvh_list->Append(std::move(descriptor));
  }
  data.Set(kPagesField, std::move(rvh_list));

  std::unique_ptr<base::ListValue> browser_list(new base::ListValue());
#if !defined(OS_ANDROID)
  for (Browser* browser : *BrowserList::GetInstance()) {
    browser_list->Append(BuildTargetDescriptor(browser));
  }
#endif  // !defined(OS_ANDROID)
  data.Set(kBrowsersField, std::move(browser_list));

  data.SetBoolean(kImprovementsEnabledField, improvements_enabled);

  std::string json_string;
  base::JSONWriter::Write(data, &json_string);

  callback.Run(base::RefCountedString::TakeString(&json_string));
}

std::string RecursiveDumpAXPlatformNodeAsString(ui::AXPlatformNode* node,
                                                int indent) {
  if (!node)
    return "";
  std::string str(2 * indent, '+');
  str += node->GetDelegate()->GetData().ToString() + "\n";
  for (int i = 0; i < node->GetDelegate()->GetChildCount(); i++) {
    gfx::NativeViewAccessible child = node->GetDelegate()->ChildAtIndex(i);
    ui::AXPlatformNode* child_node =
        ui::AXPlatformNode::FromNativeViewAccessible(child);
    str += RecursiveDumpAXPlatformNodeAsString(child_node, indent + 1);
  }
  return str;
}

// Add property filters to the property_filters vector for the given property
// filter type. The attributes are passed in as a string with each attribute
// separated by a space.
void AddPropertyFilters(
    std::vector<content::AccessibilityTreeFormatter::PropertyFilter>&
        property_filters,
    const std::string& attributes,
    content::AccessibilityTreeFormatter::PropertyFilter::Type type) {
  for (const std::string& attribute : base::SplitString(
           attributes, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    property_filters.push_back(
        content::AccessibilityTreeFormatter::PropertyFilter(
            base::ASCIIToUTF16(attribute), type));
  }
}

std::string Validate(const std::string* str) {
  CHECK(str);
  return *str;
}

}  // namespace

AccessibilityUI::AccessibilityUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://accessibility source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIAccessibilityHost);

  const bool improvements_enabled = base::FeatureList::IsEnabled(
      features::kAccessibilityInternalsPageImprovements);

  // Add required resources.
  html_source->UseStringsJs();
  html_source->AddResourcePath("accessibility.css", IDR_ACCESSIBILITY_CSS);
  html_source->AddResourcePath("accessibility.js", IDR_ACCESSIBILITY_JS);
  html_source->SetDefaultResource(IDR_ACCESSIBILITY_HTML);
  html_source->SetRequestFilter(
      base::BindRepeating(&ShouldHandleAccessibilityRequestCallback),
      base::Bind(&HandleAccessibilityRequestCallback,
                 web_ui->GetWebContents()->GetBrowserContext(),
                 improvements_enabled));

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, html_source);

  web_ui->AddMessageHandler(
      std::make_unique<AccessibilityUIMessageHandler>(improvements_enabled));
}

AccessibilityUI::~AccessibilityUI() {}

AccessibilityUIMessageHandler::AccessibilityUIMessageHandler(
    bool improvements_enabled)
    : improvements_enabled_(base::FeatureList::IsEnabled(
          features::kAccessibilityInternalsPageImprovements)) {}

AccessibilityUIMessageHandler::~AccessibilityUIMessageHandler() {}

void AccessibilityUIMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "toggleAccessibility",
      base::BindRepeating(&AccessibilityUIMessageHandler::ToggleAccessibility,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setGlobalFlag",
      base::BindRepeating(&AccessibilityUIMessageHandler::SetGlobalFlag,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestWebContentsTree",
      base::BindRepeating(
          &AccessibilityUIMessageHandler::RequestWebContentsTree,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestNativeUITree",
      base::BindRepeating(&AccessibilityUIMessageHandler::RequestNativeUITree,
                          base::Unretained(this)));
}

void AccessibilityUIMessageHandler::ToggleAccessibility(
    const base::ListValue* args) {
  const base::DictionaryValue* data;
  CHECK(args->GetDictionary(0, &data));

  int process_id = *data->FindIntPath(kProcessIdField);
  int route_id = *data->FindIntPath(kRouteIdField);
  int mode = *data->FindIntPath(kModeIdField);
  bool should_request_tree = *data->FindBoolPath(kShouldRequestTreeField);

  AllowJavascript();
  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(process_id, route_id);
  if (!rvh)
    return;
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  ui::AXMode current_mode = web_contents->GetAccessibilityMode();

  if (mode & ui::AXMode::kNativeAPIs)
    current_mode.set_mode(ui::AXMode::kNativeAPIs, true);

  if (mode & ui::AXMode::kWebContents)
    current_mode.set_mode(ui::AXMode::kWebContents, true);

  if (mode & ui::AXMode::kInlineTextBoxes)
    current_mode.set_mode(ui::AXMode::kInlineTextBoxes, true);

  if (mode & ui::AXMode::kScreenReader)
    current_mode.set_mode(ui::AXMode::kScreenReader, true);

  if (mode & ui::AXMode::kHTML)
    current_mode.set_mode(ui::AXMode::kHTML, true);

  if (mode & ui::AXMode::kLabelImages)
    current_mode.set_mode(ui::AXMode::kLabelImages, true);

  web_contents->SetAccessibilityMode(current_mode);

  if (should_request_tree) {
    base::DictionaryValue request_data;
    request_data.SetIntPath(kProcessIdField, process_id);
    request_data.SetIntPath(kRouteIdField, route_id);
    request_data.SetStringPath(kRequestTypeField, kShowTree);
    base::ListValue request_args;
    request_args.Append(std::move(request_data));
    RequestWebContentsTree(&request_args);
  } else {
    // Call accessibility.showTree without a 'tree' field so the row's
    // accessibility mode buttons are updated.
    AllowJavascript();
    std::unique_ptr<base::DictionaryValue> new_mode(BuildTargetDescriptor(rvh));
    CallJavascriptFunction("accessibility.showTree", *(new_mode.get()));
  }
}

void AccessibilityUIMessageHandler::SetGlobalFlag(const base::ListValue* args) {
  const base::DictionaryValue* data;
  CHECK(args->GetDictionary(0, &data));

  std::string flag_name_str = Validate(data->FindStringPath(kFlagNameField));
  bool enabled = *data->FindBoolPath(kEnabledField);

  AllowJavascript();
  if (flag_name_str == kInternal) {
    PrefService* pref = Profile::FromWebUI(web_ui())->GetPrefs();
    pref->SetBoolean(prefs::kShowInternalAccessibilityTree, enabled);
    return;
  }

  ui::AXMode new_mode;
  if (flag_name_str == kNative) {
    new_mode = ui::AXMode::kNativeAPIs;
  } else if (flag_name_str == kWeb) {
    new_mode = ui::AXMode::kWebContents;
  } else if (flag_name_str == kText) {
    new_mode = ui::AXMode::kInlineTextBoxes;
  } else if (flag_name_str == kScreenReader) {
    new_mode = ui::AXMode::kScreenReader;
  } else if (flag_name_str == kHTML) {
    new_mode = ui::AXMode::kHTML;
  } else if (flag_name_str == kLabelImages) {
    new_mode = ui::AXMode::kLabelImages;
  } else {
    NOTREACHED();
    return;
  }

  // It doesn't make sense to enable one of the flags that depends on
  // web contents without enabling web contents accessibility too.
  if (enabled && (new_mode.has_mode(ui::AXMode::kInlineTextBoxes) ||
                  new_mode.has_mode(ui::AXMode::kScreenReader) ||
                  new_mode.has_mode(ui::AXMode::kHTML) ||
                  new_mode.has_mode(ui::AXMode::kLabelImages))) {
    new_mode.set_mode(ui::AXMode::kWebContents, true);
  }

  // Similarly if you disable web accessibility we should remove all
  // flags that depend on it.
  if (!enabled && new_mode.has_mode(ui::AXMode::kWebContents)) {
    new_mode.set_mode(ui::AXMode::kInlineTextBoxes, true);
    new_mode.set_mode(ui::AXMode::kScreenReader, true);
    new_mode.set_mode(ui::AXMode::kHTML, true);
  }

  content::BrowserAccessibilityState* state =
      content::BrowserAccessibilityState::GetInstance();
  if (enabled)
    state->AddAccessibilityModeFlags(new_mode);
  else
    state->RemoveAccessibilityModeFlags(new_mode);
}

void AccessibilityUIMessageHandler::RequestWebContentsTree(
    const base::ListValue* args) {
  const base::DictionaryValue* data;
  CHECK(args->GetDictionary(0, &data));

  int process_id = *data->FindIntPath(kProcessIdField);
  int route_id = *data->FindIntPath(kRouteIdField);

  std::string request_type = Validate(data->FindStringPath(kRequestTypeField));
  CHECK(request_type == kShowTree || request_type == kCopyTree);
  request_type = "accessibility." + request_type;

  std::string allow = Validate(data->FindStringPath("filters.allow"));
  std::string allow_empty =
      Validate(data->FindStringPath("filters.allowEmpty"));
  std::string deny = Validate(data->FindStringPath("filters.deny"));

  AllowJavascript();
  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(process_id, route_id);
  if (!rvh) {
    std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
    result->SetInteger(kProcessIdField, process_id);
    result->SetInteger(kRouteIdField, route_id);
    result->SetString(kErrorField, "Renderer no longer exists.");
    CallJavascriptFunction(request_type, *(result.get()));
    return;
  }

  std::unique_ptr<base::DictionaryValue> result(BuildTargetDescriptor(rvh));
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  // No matter the state of the current web_contents, we want to force the mode
  // because we are about to show the accessibility tree
  web_contents->SetAccessibilityMode(
      ui::AXMode(ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents));

  std::vector<content::AccessibilityTreeFormatter::PropertyFilter>
      property_filters;
  AddPropertyFilters(
      property_filters, allow,
      content::AccessibilityTreeFormatter::PropertyFilter::ALLOW);
  AddPropertyFilters(
      property_filters, allow_empty,
      content::AccessibilityTreeFormatter::PropertyFilter::ALLOW_EMPTY);
  AddPropertyFilters(property_filters, deny,
                     content::AccessibilityTreeFormatter::PropertyFilter::DENY);

  PrefService* pref = Profile::FromWebUI(web_ui())->GetPrefs();
  bool internal = pref->GetBoolean(prefs::kShowInternalAccessibilityTree);
  base::string16 accessibility_contents_utf16 =
      web_contents->DumpAccessibilityTree(internal, property_filters);
  result->SetString(kTreeField,
                    base::UTF16ToUTF8(accessibility_contents_utf16));
  result->SetBoolean(kImprovementsEnabledField, improvements_enabled_);
  CallJavascriptFunction(request_type, *(result.get()));
}

void AccessibilityUIMessageHandler::RequestNativeUITree(
    const base::ListValue* args) {
  const base::DictionaryValue* data;
  CHECK(args->GetDictionary(0, &data));

  int session_id = *data->FindIntPath(kSessionIdField);
  std::string request_type = Validate(data->FindStringPath(kRequestTypeField));
  CHECK(request_type == kShowTree || request_type == kCopyTree);
  request_type = "accessibility." + request_type;

  AllowJavascript();

#if !defined(OS_ANDROID)
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->session_id().id() == session_id) {
      std::unique_ptr<base::DictionaryValue> result(
          BuildTargetDescriptor(browser));
      gfx::NativeWindow native_window = browser->window()->GetNativeWindow();
      ui::AXPlatformNode* node =
          ui::AXPlatformNode::FromNativeWindow(native_window);
      result->SetKey(kTreeField,
                     base::Value(RecursiveDumpAXPlatformNodeAsString(node, 0)));
      CallJavascriptFunction(request_type, *(result.get()));
      return;
    }
  }
#endif  // !defined(OS_ANDROID)
  // No browser with the specified |session_id| was found.
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetInteger(kSessionIdField, session_id);
  result->SetString(kTypeField, kBrowser);
  result->SetString(kErrorField, "Browser no longer exists.");
  CallJavascriptFunction(request_type, *(result.get()));
}

// static
void AccessibilityUIMessageHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kShowInternalAccessibilityTree, false);
}
