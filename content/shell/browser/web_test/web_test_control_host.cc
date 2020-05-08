// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_control_host.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/paint/skia_paint_canvas.h"
#include "content/common/page_state_serialization.h"
#include "content/common/unique_name_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/blink_test_browser_support.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_devtools_frontend.h"
#include "content/shell/browser/web_test/devtools_protocol_test_bindings.h"
#include "content/shell/browser/web_test/fake_bluetooth_chooser.h"
#include "content/shell/browser/web_test/mock_client_hints_controller_delegate.h"
#include "content/shell/browser/web_test/test_info_extractor.h"
#include "content/shell/browser/web_test/web_test_bluetooth_chooser_factory.h"
#include "content/shell/browser/web_test/web_test_content_browser_client.h"
#include "content/shell/browser/web_test/web_test_devtools_bindings.h"
#include "content/shell/browser/web_test/web_test_first_device_bluetooth_chooser.h"
#include "content/shell/common/web_test/web_test_string_util.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "content/test/storage_partition_test_helpers.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif

namespace content {

namespace {

std::string DumpFrameState(const ExplodedFrameState& frame_state,
                           size_t indent,
                           bool is_current_index) {
  std::string result;
  if (is_current_index) {
    constexpr const char kCurrentMarker[] = "curr->";
    result.append(kCurrentMarker);
    result.append(indent - strlen(kCurrentMarker), ' ');
  } else {
    result.append(indent, ' ');
  }

  std::string url = web_test_string_util::NormalizeWebTestURL(
      base::UTF16ToUTF8(frame_state.url_string.value_or(base::string16())));
  result.append(url);
  DCHECK(frame_state.target);
  if (!frame_state.target->empty()) {
    std::string unique_name = base::UTF16ToUTF8(*frame_state.target);
    result.append(" (in frame \"");
    result.append(UniqueNameHelper::ExtractStableNameForTesting(unique_name));
    result.append("\")");
  }
  result.append("\n");

  std::vector<ExplodedFrameState> sorted_children = frame_state.children;
  std::sort(
      sorted_children.begin(), sorted_children.end(),
      [](const ExplodedFrameState& lhs, const ExplodedFrameState& rhs) {
        // Child nodes should always have a target (aka unique name).
        DCHECK(lhs.target);
        DCHECK(rhs.target);
        std::string lhs_name = UniqueNameHelper::ExtractStableNameForTesting(
            base::UTF16ToUTF8(*lhs.target));
        std::string rhs_name = UniqueNameHelper::ExtractStableNameForTesting(
            base::UTF16ToUTF8(*rhs.target));
        if (!base::EqualsCaseInsensitiveASCII(lhs_name, rhs_name))
          return base::CompareCaseInsensitiveASCII(lhs_name, rhs_name) < 0;

        return lhs.item_sequence_number < rhs.item_sequence_number;
      });
  for (const auto& child : sorted_children)
    result += DumpFrameState(child, indent + 4, false);

  return result;
}

std::string DumpNavigationEntry(NavigationEntry* navigation_entry,
                                bool is_current_index) {
  // This is silly, but it's currently the best way to extract the information.
  PageState page_state = navigation_entry->GetPageState();
  ExplodedPageState exploded_page_state;
  CHECK(DecodePageState(page_state.ToEncodedData(), &exploded_page_state));
  return DumpFrameState(exploded_page_state.top, 8, is_current_index);
}

std::string DumpHistoryForWebContents(WebContents* web_contents) {
  std::string result;
  const int current_index =
      web_contents->GetController().GetCurrentEntryIndex();
  for (int i = 0; i < web_contents->GetController().GetEntryCount(); ++i) {
    result += DumpNavigationEntry(
        web_contents->GetController().GetEntryAtIndex(i), i == current_index);
  }
  return result;
}

std::vector<std::string> DumpTitleWasSet(WebContents* web_contents) {
  base::Optional<bool> load = WebTestControlHost::Get()
                                  ->accumulated_web_test_runtime_flags_changes()
                                  .FindBoolPath("dump_frame_load_callbacks");

  base::Optional<bool> title_changed =
      WebTestControlHost::Get()
          ->accumulated_web_test_runtime_flags_changes()
          .FindBoolPath("dump_title_changes");

  std::vector<std::string> logs;

  if (load.has_value() && load.value()) {
    // TitleWasSet is only available on top-level frames.
    std::string log = "main frame";
    logs.emplace_back(
        log + " - TitleWasSet: " + base::UTF16ToUTF8(web_contents->GetTitle()));
  }

  if (title_changed.has_value() && title_changed.value()) {
    logs.emplace_back("TITLE CHANGED: '" +
                      base::UTF16ToUTF8(web_contents->GetTitle()) + "'");
  }
  return logs;
}

std::string DumpFailLoad(WebContents* web_contents,
                         RenderFrameHost* render_frame_host) {
  base::Optional<bool> result =
      WebTestControlHost::Get()
          ->accumulated_web_test_runtime_flags_changes()
          .FindBoolPath("dump_frame_load_callbacks");

  if (!result.has_value())
    return std::string();

  std::string log = (web_contents->GetMainFrame() == render_frame_host)
                        ? "main frame "
                        : "frame ";
  std::string name = GetFrameNameFromBrowserForWebTests(render_frame_host);
  log += !name.empty() ? "\"" + name + "\"" : "(anonymous)";
  return log + " - DidFailLoad";
}

// Draws a selection rect into a bitmap.
void DrawSelectionRect(const SkBitmap& bitmap, const blink::WebRect& wr) {
  // Render a red rectangle bounding selection rect
  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setColor(0xFFFF0000);  // Fully opaque red
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  flags.setStrokeWidth(1.0f);
  SkIRect rect;  // Bounding rect
  rect.setXYWH(wr.x, wr.y, wr.width, wr.height);
  canvas.drawIRect(rect, flags);
}

// Applies settings that differ between web tests and regular mode. Some
// of the defaults are controlled via command line flags which are
// automatically set for web tests.
void ApplyWebTestDefaultPreferences(WebPreferences* prefs) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  prefs->allow_universal_access_from_file_urls = false;
  prefs->dom_paste_enabled = true;
  prefs->javascript_can_access_clipboard = true;
  prefs->xslt_enabled = true;
  prefs->application_cache_enabled = true;
  prefs->tabs_to_links = false;
  prefs->hyperlink_auditing_enabled = false;
  prefs->allow_running_insecure_content = false;
  prefs->disable_reading_from_canvas = false;
  prefs->strict_mixed_content_checking = false;
  prefs->strict_powerful_feature_restrictions = false;
  prefs->webgl_errors_to_console_enabled = false;
  prefs->enable_scroll_animator =
      !command_line.HasSwitch(switches::kDisableSmoothScrolling);
  prefs->minimum_logical_font_size = 9;
  prefs->accelerated_2d_canvas_enabled =
      command_line.HasSwitch(switches::kEnableAccelerated2DCanvas);
  prefs->smart_insert_delete_enabled = true;
  prefs->viewport_enabled = command_line.HasSwitch(switches::kEnableViewport);
  prefs->default_minimum_page_scale_factor = 1.f;
  prefs->default_maximum_page_scale_factor = 4.f;
  prefs->presentation_receiver =
      command_line.HasSwitch(switches::kForcePresentationReceiverForTesting);
  prefs->translate_service_available = true;

#if defined(OS_MACOSX)
  prefs->editing_behavior = EDITING_BEHAVIOR_MAC;
#else
  prefs->editing_behavior = EDITING_BEHAVIOR_WIN;
#endif

#if defined(OS_MACOSX)
  prefs->cursive_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("Apple Chancery");
  prefs->fantasy_font_family_map[kCommonScript] = base::ASCIIToUTF16("Papyrus");
  prefs->serif_font_family_map[kCommonScript] = base::ASCIIToUTF16("Times");
  prefs->standard_font_family_map[kCommonScript] = base::ASCIIToUTF16("Times");
#else
  prefs->cursive_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("Comic Sans MS");
  prefs->fantasy_font_family_map[kCommonScript] = base::ASCIIToUTF16("Impact");
  prefs->serif_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("times new roman");
  prefs->standard_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("times new roman");
#endif
  prefs->fixed_font_family_map[kCommonScript] = base::ASCIIToUTF16("Courier");
  prefs->sans_serif_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("Helvetica");
}

}  // namespace

// WebTestResultPrinter ----------------------------------------------------

WebTestResultPrinter::WebTestResultPrinter(std::ostream* output,
                                           std::ostream* error)
    : state_(DURING_TEST),
      capture_text_only_(false),
      encode_binary_data_(false),
      output_(output),
      error_(error) {}

void WebTestResultPrinter::StartStateDump() {
  state_ = DURING_STATE_DUMP;
}

void WebTestResultPrinter::PrintTextHeader() {
  if (state_ != DURING_STATE_DUMP)
    return;
  if (!capture_text_only_)
    *output_ << "Content-Type: text/plain\n";
  state_ = IN_TEXT_BLOCK;
}

void WebTestResultPrinter::PrintTextBlock(const std::string& block) {
  if (state_ != IN_TEXT_BLOCK)
    return;
  *output_ << block;
}

void WebTestResultPrinter::PrintTextFooter() {
  if (state_ != IN_TEXT_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    output_->flush();
  }
  state_ = IN_IMAGE_BLOCK;
}

void WebTestResultPrinter::PrintImageHeader(const std::string& actual_hash,
                                            const std::string& expected_hash) {
  if (state_ != IN_IMAGE_BLOCK || capture_text_only_)
    return;
  *output_ << "\nActualHash: " << actual_hash << "\n";
  if (!expected_hash.empty())
    *output_ << "\nExpectedHash: " << expected_hash << "\n";
}

void WebTestResultPrinter::PrintImageBlock(
    const std::vector<unsigned char>& png_image) {
  if (state_ != IN_IMAGE_BLOCK || capture_text_only_)
    return;
  *output_ << "Content-Type: image/png\n";
  if (encode_binary_data_) {
    PrintEncodedBinaryData(png_image);
    return;
  }

  *output_ << "Content-Length: " << png_image.size() << "\n";
  output_->write(reinterpret_cast<const char*>(&png_image[0]),
                 png_image.size());
}

void WebTestResultPrinter::PrintImageFooter() {
  if (state_ != IN_IMAGE_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    output_->flush();
  }
  state_ = AFTER_TEST;
}

void WebTestResultPrinter::PrintAudioHeader() {
  DCHECK_EQ(state_, DURING_STATE_DUMP);
  if (!capture_text_only_)
    *output_ << "Content-Type: audio/wav\n";
  state_ = IN_AUDIO_BLOCK;
}

void WebTestResultPrinter::PrintAudioBlock(
    const std::vector<unsigned char>& audio_data) {
  if (state_ != IN_AUDIO_BLOCK || capture_text_only_)
    return;
  if (encode_binary_data_) {
    PrintEncodedBinaryData(audio_data);
    return;
  }

  *output_ << "Content-Length: " << audio_data.size() << "\n";
  output_->write(reinterpret_cast<const char*>(&audio_data[0]),
                 audio_data.size());
}

void WebTestResultPrinter::PrintAudioFooter() {
  if (state_ != IN_AUDIO_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    output_->flush();
  }
  state_ = IN_IMAGE_BLOCK;
}

void WebTestResultPrinter::AddMessageToStderr(const std::string& message) {
  *error_ << message;
}

void WebTestResultPrinter::AddMessage(const std::string& message) {
  AddMessageRaw(message + "\n");
}

void WebTestResultPrinter::AddMessageRaw(const std::string& message) {
  if (state_ != DURING_TEST)
    return;
  *output_ << message;
}

void WebTestResultPrinter::AddErrorMessage(const std::string& message) {
  if (!capture_text_only_)
    *error_ << message << "\n";
  if (state_ != DURING_TEST && state_ != DURING_STATE_DUMP)
    return;
  PrintTextHeader();
  *output_ << message << "\n";
  PrintTextFooter();
  PrintImageFooter();
}

void WebTestResultPrinter::PrintEncodedBinaryData(
    const std::vector<unsigned char>& data) {
  *output_ << "Content-Transfer-Encoding: base64\n";

  std::string data_base64;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(&data[0]), data.size()),
      &data_base64);

  *output_ << "Content-Length: " << data_base64.length() << "\n";
  output_->write(data_base64.c_str(), data_base64.length());
}

void WebTestResultPrinter::CloseStderr() {
  if (state_ != AFTER_TEST)
    return;
  if (!capture_text_only_) {
    *error_ << "#EOF\n";
    error_->flush();
  }
}

// WebTestControlHost -------------------------------------------------------

WebTestControlHost* WebTestControlHost::instance_ = nullptr;

// static
WebTestControlHost* WebTestControlHost::Get() {
  return instance_;
}

WebTestControlHost::WebTestControlHost()
    : main_window_(nullptr),
      secondary_window_(nullptr),
      test_phase_(BETWEEN_TESTS),
      crash_when_leak_found_(false),
      pending_layout_dumps_(0) {
  CHECK(!instance_);
  instance_ = this;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLeakDetection)) {
    leak_detector_ = std::make_unique<LeakDetector>();
    std::string switchValue =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnableLeakDetection);
    crash_when_leak_found_ = switchValue == switches::kCrashOnFailure;
  }

  printer_ = std::make_unique<WebTestResultPrinter>(&std::cout, &std::cerr);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEncodeBinary))
    printer_->set_encode_binary_data(true);

  // Print text only (without binary dumps and headers/footers for run_web_tests
  // protocol) until we enter the protocol mode (see TestInfo::protocol_mode).
  printer_->set_capture_text_only(true);

  InjectTestSharedWorkerService(BrowserContext::GetStoragePartition(
      ShellContentBrowserClient::Get()->browser_context(), nullptr));

  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CREATED,
                 NotificationService::AllSources());
  GpuDataManager::GetInstance()->AddObserver(this);
  ResetBrowserAfterWebTest();
}

WebTestControlHost::~WebTestControlHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(instance_ == this);
  CHECK(test_phase_ == BETWEEN_TESTS);
  GpuDataManager::GetInstance()->RemoveObserver(this);
  DiscardMainWindow();
  instance_ = nullptr;
}

bool WebTestControlHost::PrepareForWebTest(const TestInfo& test_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  test_phase_ = DURING_TEST;
  current_working_directory_ = test_info.current_working_directory;
  expected_pixel_hash_ = test_info.expected_pixel_hash;
  bool is_devtools_js_test = false;
  test_url_ = WebTestDevToolsBindings::MapTestURLIfNeeded(test_info.url,
                                                          &is_devtools_js_test);
  bool is_devtools_protocol_test = false;
  test_url_ = DevToolsProtocolTestBindings::MapTestURLIfNeeded(
      test_url_, &is_devtools_protocol_test);
  did_send_initial_test_configuration_ = false;

  protocol_mode_ = test_info.protocol_mode;
  if (protocol_mode_)
    printer_->set_capture_text_only(false);
  printer_->reset();

  web_test_render_frame_map_.clear();
  web_test_render_thread_map_.clear();
  accumulated_web_test_runtime_flags_changes_.Clear();
  main_window_render_view_hosts_.clear();
  main_window_render_process_hosts_.clear();
  all_observed_render_process_hosts_.clear();
  render_process_host_observer_.RemoveAll();
  frame_to_layout_dump_map_.clear();

  ShellBrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();

  browser_context->GetClientHintsControllerDelegate()->ResetForTesting();

  initial_size_ = Shell::GetShellDefaultSize();
  if (!main_window_) {
    main_window_ = content::Shell::CreateNewWindow(
        browser_context, GURL(url::kAboutBlankURL), nullptr, initial_size_);
    WebContentsObserver::Observe(main_window_->web_contents());

    current_pid_ = base::kNullProcessId;
    default_prefs_ = main_window_->web_contents()
                         ->GetRenderViewHost()
                         ->GetWebkitPreferences();
  } else {
#if defined(OS_MACOSX)
    // Shell::SizeTo is not implemented on all platforms.
    main_window_->SizeTo(initial_size_);
#endif
    RenderViewHost* render_view_host =
        main_window_->web_contents()->GetRenderViewHost();
    RenderWidgetHost* render_widget_host = render_view_host->GetWidget();
    // Set a different size first to reset the possibly inconsistent state
    // caused by the previous test using unfortunate synchronous resize mode.
    // This forces SetSize() not to early return which would otherwise happen
    // when we set the size to initial_size_ which is the same as its current
    // size. See http://crbug.com/1011191 for more details.
    render_widget_host->GetView()->SetSize(
        gfx::Size(initial_size_.width() / 2, initial_size_.height()));
    render_widget_host->GetView()->SetSize(initial_size_);
    render_widget_host->SynchronizeVisualProperties();

    render_view_host->UpdateWebkitPreferences(default_prefs_);
  }

  if (is_devtools_js_test && !secondary_window_) {
    secondary_window_ = content::Shell::CreateNewWindow(
        ShellContentBrowserClient::Get()->browser_context(),
        GURL(url::kAboutBlankURL), nullptr, initial_size_);
  }

  // The main frame is constructed along with the Shell, which is before we can
  // observe it happening. Further, we clear all observers and re-add the main
  // frame here before each test.
  HandleNewRenderFrameHost(main_window_->web_contents()->GetMainFrame());
  if (secondary_window_)
    HandleNewRenderFrameHost(secondary_window_->web_contents()->GetMainFrame());

  if (is_devtools_protocol_test) {
    devtools_protocol_test_bindings_ =
        std::make_unique<DevToolsProtocolTestBindings>(
            main_window_->web_contents());
  }

  // We don't go down the normal system path of focusing RenderWidgetHostView
  // because on mac headless, there are no system windows and that path does
  // not do anything. Instead we go through the Shell::ActivateContents() path
  // which knows how to perform the activation correctly on all platforms and in
  // headless mode.
  main_window_->ActivateContents(main_window_->web_contents());

  // Flush various interfaces to ensure a test run begins from a known
  // state.
  main_window_->web_contents()
      ->GetRenderViewHost()
      ->GetWidget()
      ->FlushForTesting();
  GetWebTestRenderFrameRemote(
      main_window_->web_contents()->GetRenderViewHost()->GetMainFrame())
      .FlushForTesting();

  if (is_devtools_js_test) {
    // This navigates the secondary (devtools inspector) window, and then
    // navigates the main window once that has loaded to a devtools html test
    // page, based on the test url.
    devtools_bindings_ = std::make_unique<WebTestDevToolsBindings>(
        main_window_->web_contents(), secondary_window_->web_contents(),
        test_url_);
  } else {
    // Loading the URL will immediately start the web test. Manually call
    // LoadURLWithParams on the WebContents to avoid extraneous calls from
    // content::Shell such as SetFocus(), which could race with the web
    // test.
    NavigationController::LoadURLParams params(test_url_);

    // Using PAGE_TRANSITION_TYPED replicates an omnibox navigation.
    params.transition_type =
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED);

    // Clear history to purge the prior navigation to about:blank.
    params.should_clear_history_list = true;
    main_window_->web_contents()->GetController().LoadURLWithParams(params);
  }

  return true;
}

bool WebTestControlHost::ResetBrowserAfterWebTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  printer_->PrintTextFooter();
  printer_->PrintImageFooter();
  printer_->CloseStderr();
  did_send_initial_test_configuration_ = false;
  test_phase_ = BETWEEN_TESTS;
  expected_pixel_hash_.clear();
  test_url_ = GURL();
  prefs_ = WebPreferences();
  should_override_prefs_ = false;
  WebTestContentBrowserClient::Get()->SetPopupBlockingEnabled(false);
  WebTestContentBrowserClient::Get()->ResetMockClipboardHosts();
  WebTestContentBrowserClient::Get()->SetScreenOrientationChanged(false);
  WebTestContentBrowserClient::Get()->ResetFakeBluetoothDelegate();
  navigation_history_dump_ = "";
  pixel_dump_.reset();
  blink_test_client_receivers_.Clear();
  actual_pixel_hash_ = "";
  main_frame_dump_ = nullptr;
  waiting_for_pixel_results_ = false;
  waiting_for_main_frame_dump_ = false;
  composite_all_frames_node_queue_ = std::queue<Node*>();
  composite_all_frames_node_storage_.clear();
  ui::SelectFileDialog::SetFactory(nullptr);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (writable_directory_for_tests_.IsValid()) {
      if (!writable_directory_for_tests_.Delete())
        LOG(ERROR) << "Failed to delete temporary directory";
    }
  }
  weak_factory_.InvalidateWeakPtrs();

  return true;
}

void WebTestControlHost::SetTempPath(const base::FilePath& temp_path) {
  temp_path_ = temp_path;
}

void WebTestControlHost::RendererUnresponsive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHangMonitor)) {
    LOG(WARNING) << "renderer unresponsive";
  }
}

void WebTestControlHost::OverrideWebkitPrefs(WebPreferences* prefs) {
  if (should_override_prefs_) {
    *prefs = prefs_;
  } else {
    ApplyWebTestDefaultPreferences(prefs);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode)) {
    prefs->preferred_color_scheme = blink::PreferredColorScheme::kDark;
  } else {
    prefs->preferred_color_scheme = blink::PreferredColorScheme::kLight;
  }
}

void WebTestControlHost::OpenURL(const GURL& url) {
  if (test_phase_ != DURING_TEST)
    return;

  Shell::CreateNewWindow(main_window_->web_contents()->GetBrowserContext(), url,
                         main_window_->web_contents()->GetSiteInstance(),
                         gfx::Size());
}

void WebTestControlHost::InitiateLayoutDump() {
  // There should be at most 1 layout dump in progress at any given time.
  DCHECK_EQ(0, pending_layout_dumps_);

  int number_of_messages = 0;
  for (RenderFrameHost* rfh : main_window_->web_contents()->GetAllFrames()) {
    if (!rfh->IsRenderFrameLive())
      continue;

    ++number_of_messages;
    GetWebTestRenderFrameRemote(rfh)->DumpFrameLayout(
        base::BindOnce(&WebTestControlHost::OnDumpFrameLayoutResponse,
                       weak_factory_.GetWeakPtr(), rfh->GetFrameTreeNodeId()));
  }

  pending_layout_dumps_ = number_of_messages;
}

void WebTestControlHost::InitiateCaptureDump(bool capture_navigation_history,
                                             bool capture_pixels) {
  if (test_phase_ != DURING_TEST)
    return;

  if (capture_navigation_history) {
    RenderFrameHost* main_rfh = main_window_->web_contents()->GetMainFrame();
    for (auto* window : Shell::windows()) {
      WebContents* web_contents = window->web_contents();
      // Only capture the history from windows in the same process_host as the
      // main window. During web tests, we only use two processes when a
      // devtools window is open.
      // TODO(https://crbug.com/771003): Dump history for all WebContentses, not
      // just ones that happen to be in the same process_host as the main test
      // window's main frame.
      if (main_rfh->GetProcess() != web_contents->GetMainFrame()->GetProcess())
        continue;

      navigation_history_dump_ +=
          "\n============== Back Forward List ==============\n";
      navigation_history_dump_ += DumpHistoryForWebContents(web_contents);
      navigation_history_dump_ +=
          "===============================================\n";
    }
  }

  // Ensure to say that we need to wait for main frame dump here, since
  // CopyFromSurface call below may synchronously issue the callback, meaning
  // that we would report results too early.
  waiting_for_main_frame_dump_ = true;

  if (capture_pixels) {
    waiting_for_pixel_results_ = true;
    auto* rwhv = main_window_->web_contents()->GetRenderWidgetHostView();
    // If we're running in threaded mode, then the frames will be produced via a
    // scheduler elsewhere, all we need to do is to ensure that the surface is
    // synchronized before we copy from it. In single threaded mode, we have to
    // force each renderer to produce a frame.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableThreadedCompositing)) {
      rwhv->EnsureSurfaceSynchronizedForWebTest();
      EnqueueSurfaceCopyRequest();
    } else {
      CompositeAllFramesThen(
          base::BindOnce(&WebTestControlHost::EnqueueSurfaceCopyRequest,
                         weak_factory_.GetWeakPtr()));
    }
  }

  RenderFrameHost* rfh = main_window_->web_contents()->GetMainFrame();
  printer_->StartStateDump();
  GetWebTestRenderFrameRemote(rfh)->CaptureDump(base::BindOnce(
      &WebTestControlHost::OnCaptureDumpCompleted, weak_factory_.GetWeakPtr()));
}

void WebTestControlHost::TestFinishedInSecondaryRenderer() {
  GetWebTestRenderFrameRemote(
      main_window_->web_contents()->GetRenderViewHost()->GetMainFrame())
      ->FinishTestInMainWindow();
}

// Enqueue an image copy output request.
void WebTestControlHost::EnqueueSurfaceCopyRequest() {
  auto* rwhv = main_window_->web_contents()->GetRenderWidgetHostView();
  rwhv->CopyFromSurface(gfx::Rect(), gfx::Size(),
                        base::BindOnce(&WebTestControlHost::OnPixelDumpCaptured,
                                       weak_factory_.GetWeakPtr()));
}

void WebTestControlHost::CompositeAllFramesThen(
    base::OnceCallback<void()> callback) {
  // Only allow a single call to CompositeAllFramesThen(), without a call to
  // ResetBrowserAfterWebTest() in between. More than once risks overlapping
  // calls, due to the asynchronous nature of CompositeNodeQueueThen(), which
  // can lead to use-after-free, e.g.
  // https://clusterfuzz.com/v2/testcase-detail/4929420383748096
  if (!composite_all_frames_node_storage_.empty() ||
      !composite_all_frames_node_queue_.empty()) {
    // Using NOTREACHED + return here because we want to disallow the second
    // call if this happens in release builds, while still catching this
    // condition in debug builds.
    NOTREACHED();
    return;
  }
  // Build the frame storage and depth first queue.
  Node* root = BuildFrameTree(main_window_->web_contents());
  BuildDepthFirstQueue(root);
  // Now asynchronously run through the node queue.
  CompositeNodeQueueThen(std::move(callback));
}

void WebTestControlHost::CompositeNodeQueueThen(
    base::OnceCallback<void()> callback) {
  // Frames can get freed somewhere else while a CompositeWithRaster is taking
  // place. Therefore, we need to double-check that this frame pointer is
  // still valid before using it. To do that, grab the list of all frames
  // again, and make sure it contains the one we're about to composite.
  // See crbug.com/899465 for an example of this problem.
  RenderFrameHost* next_node_host;
  do {
    if (composite_all_frames_node_queue_.empty()) {
      // Done with the queue - call the callback.
      std::move(callback).Run();
      return;
    }
    next_node_host =
        composite_all_frames_node_queue_.front()->render_frame_host;
    GlobalFrameRoutingId next_node_id =
        composite_all_frames_node_queue_.front()->render_frame_host_id;
    composite_all_frames_node_queue_.pop();
    if (RenderFrameHost::FromID(next_node_id.child_id,
                                next_node_id.frame_routing_id) !=
        next_node_host) {
      next_node_host = nullptr;  // This one is now gone
    }
  } while (!next_node_host || !next_node_host->IsRenderFrameLive());
  GetWebTestRenderFrameRemote(next_node_host)
      ->CompositeWithRaster(
          base::BindOnce(&WebTestControlHost::CompositeNodeQueueThen,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebTestControlHost::BuildDepthFirstQueue(Node* node) {
  for (auto* child : node->children)
    BuildDepthFirstQueue(child);
  composite_all_frames_node_queue_.push(node);
}

WebTestControlHost::Node* WebTestControlHost::BuildFrameTree(
    WebContents* web_contents) {
  // Returns a Node for a given RenderFrameHost, or nullptr if doesn't exist.
  auto node_for_frame = [this](RenderFrameHost* rfh) {
    auto it = std::find_if(
        composite_all_frames_node_storage_.begin(),
        composite_all_frames_node_storage_.end(),
        [rfh](auto& node) { return node->render_frame_host == rfh; });
    return it == composite_all_frames_node_storage_.end() ? nullptr : it->get();
  };

  Node* outer_root = nullptr;
  std::vector<WebContents*> all_web_contents(1, web_contents);
  for (unsigned i = 0; i < all_web_contents.size(); i++) {
    WebContents* contents = all_web_contents[i];

    //  Collect all live frames in contents.
    std::vector<RenderFrameHost*> frames;
    for (auto* frame : contents->GetAllFrames()) {
      if (frame->IsRenderFrameLive())
        frames.push_back(frame);
    }

    // Add all of the frames to storage.
    for (auto* frame : frames) {
      DCHECK(!node_for_frame(frame)) << "Frame seen multiple times.";
      composite_all_frames_node_storage_.emplace_back(
          std::make_unique<Node>(frame));
    }

    // Construct a tree rooted at |root|.
    Node* root = nullptr;
    for (auto* frame : frames) {
      Node* node = node_for_frame(frame);
      DCHECK(node);
      if (!frame->GetParent()) {
        DCHECK(!root) << "Multiple roots found.";
        root = node;
      } else {
        Node* parent = node_for_frame(frame->GetParent());
        DCHECK(parent);
        parent->children.push_back(node);
      }
    }
    DCHECK(root) << "No root found.";

    // Connect the inner root to the outer node.
    if (auto* outer_frame = contents->GetOuterWebContentsFrame()) {
      Node* parent = node_for_frame(outer_frame);
      DCHECK(parent);
      parent->children.push_back(root);
    } else {
      DCHECK(!outer_root) << "Multiple outer roots found.";
      outer_root = root;
    }

    // Traverse all inner contents.
    for (auto* inner_contents : contents->GetInnerWebContents())
      all_web_contents.push_back(inner_contents);
  }
  DCHECK(outer_root) << "No outer root found";

  return outer_root;
}

bool WebTestControlHost::IsMainWindow(WebContents* web_contents) const {
  return main_window_ && web_contents == main_window_->web_contents();
}

std::unique_ptr<BluetoothChooser> WebTestControlHost::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  // TODO(https://crbug.com/509038): Remove |bluetooth_chooser_factory_| once
  // all of the Web Bluetooth tests are migrated to external/wpt/.
  if (bluetooth_chooser_factory_) {
    return bluetooth_chooser_factory_->RunBluetoothChooser(frame,
                                                           event_handler);
  }

  auto next_fake_bluetooth_chooser =
      WebTestContentBrowserClient::Get()->GetNextFakeBluetoothChooser();
  if (next_fake_bluetooth_chooser) {
    const url::Origin origin = frame->GetLastCommittedOrigin();
    DCHECK(!origin.opaque());
    next_fake_bluetooth_chooser->OnRunBluetoothChooser(event_handler, origin);
    return next_fake_bluetooth_chooser;
  }

  return std::make_unique<WebTestFirstDeviceBluetoothChooser>(event_handler);
}

void WebTestControlHost::PluginCrashed(const base::FilePath& plugin_path,
                                       base::ProcessId plugin_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  printer_->AddErrorMessage(
      base::StringPrintf("#CRASHED - plugin (pid %" CrPRIdPid ")", plugin_pid));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&WebTestControlHost::DiscardMainWindow),
                     weak_factory_.GetWeakPtr()));
}

void WebTestControlHost::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleNewRenderFrameHost(render_frame_host);
}

void WebTestControlHost::TitleWasSet(NavigationEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> logs = DumpTitleWasSet(main_window_->web_contents());
  if (logs.empty())
    return;
  for (auto log : logs)
    printer_->AddMessage(log);
}

void WebTestControlHost::DidFailLoad(RenderFrameHost* render_frame_host,
                                     const GURL& validated_url,
                                     int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string log =
      DumpFailLoad(main_window_->web_contents(), render_frame_host);
  if (log.empty())
    return;
  printer_->AddMessage(log);
}

void WebTestControlHost::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  printer_->AddErrorMessage("FAIL: main window was destroyed");
  DiscardMainWindow();
}

void WebTestControlHost::DidUpdateFaviconURL(
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  bool should_dump_icon_changes = false;
  accumulated_web_test_runtime_flags_changes_.GetBoolean(
      "dump_icon_changes", &should_dump_icon_changes);
  if (should_dump_icon_changes) {
    std::string log = IsMainWindow(web_contents()) ? "main frame " : "frame ";
    printer_->AddMessageRaw(log + "- didChangeIcons\n");
  }
}

void WebTestControlHost::RenderProcessHostDestroyed(
    RenderProcessHost* render_process_host) {
  render_process_host_observer_.Remove(render_process_host);
  all_observed_render_process_hosts_.erase(render_process_host);
  web_test_render_thread_map_.erase(render_process_host);
  main_window_render_process_hosts_.erase(render_process_host);
}

void WebTestControlHost::RenderProcessExited(
    RenderProcessHost* render_process_host,
    const ChildProcessTerminationInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (info.status) {
    case base::TerminationStatus::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TerminationStatus::TERMINATION_STATUS_STILL_RUNNING:
      break;

    case base::TerminationStatus::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TerminationStatus::TERMINATION_STATUS_LAUNCH_FAILED:
    case base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TerminationStatus::TERMINATION_STATUS_PROCESS_WAS_KILLED:
    default: {
      const base::Process& process = render_process_host->GetProcess();
      if (process.IsValid()) {
        printer_->AddErrorMessage(std::string("#CRASHED - renderer (pid ") +
                                  base::NumberToString(process.Pid()) + ")");
      } else {
        printer_->AddErrorMessage("#CRASHED - renderer");
      }

      DiscardMainWindow();
      break;
    }
  }
}

void WebTestControlHost::Observe(int type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (type) {
    case NOTIFICATION_RENDERER_PROCESS_CREATED: {
      if (!main_window_)
        return;
      RenderViewHost* render_view_host =
          main_window_->web_contents()->GetRenderViewHost();
      if (!render_view_host)
        return;
      RenderProcessHost* render_process_host =
          Source<RenderProcessHost>(source).ptr();
      if (render_process_host != render_view_host->GetProcess())
        return;
      current_pid_ = render_process_host->GetProcess().Pid();
      break;
    }
    default:
      NOTREACHED();
  }
}

void WebTestControlHost::OnGpuProcessCrashed(
    base::TerminationStatus exit_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  printer_->AddErrorMessage("#CRASHED - gpu");
  DiscardMainWindow();
}

void WebTestControlHost::DiscardMainWindow() {
  // If we're running a test, we need to close all windows and exit the message
  // loop. Otherwise, we're already outside of the message loop, and we just
  // discard the main window.
  devtools_bindings_.reset();
  devtools_protocol_test_bindings_.reset();
  WebContentsObserver::Observe(nullptr);
  if (test_phase_ != BETWEEN_TESTS) {
    // CloseAllWindows will also signal the main message loop to exit.
    Shell::CloseAllWindows();
    test_phase_ = CLEAN_UP;
  } else if (main_window_) {
    main_window_->Close();
  }
  main_window_ = nullptr;
  current_pid_ = base::kNullProcessId;
}

void WebTestControlHost::HandleNewRenderFrameHost(RenderFrameHost* frame) {
  RenderProcessHost* process_host = frame->GetProcess();
  RenderViewHost* view_host = frame->GetRenderViewHost();
  bool main_window =
      WebContents::FromRenderFrameHost(frame) == main_window_->web_contents();

  // Track pid of the renderer handling the main frame.
  if (main_window && frame->GetParent() == nullptr) {
    const base::Process& process = process_host->GetProcess();
    if (process.IsValid())
      current_pid_ = process.Pid();
  }

  // If this the first time this renderer contains parts of the main test
  // window, we need to make sure that it gets configured correctly (including
  // letting it know that it's part of the main test window).
  // We consider the renderer as new when we see either a new RenderProcessHost
  // or a new RenderViewHost, as it is possible that a new renderer (with a new
  // RenderViewHost) reuses a renderer process, and it's also possible that we
  // reuse RenderViewHosts (in some fetch tests).
  // TODO(rakina): Understand the fetch tests to figure out if it's possible to
  // remove RenderProcessHost tracking here.
  if (main_window &&
      (!base::Contains(main_window_render_view_hosts_, view_host) ||
       !base::Contains(main_window_render_process_hosts_, process_host))) {
    main_window_render_view_hosts_.insert(view_host);
    main_window_render_process_hosts_.insert(process_host);

    // Make sure the new renderer process_host has a test configuration shared
    // with other renderers.
    mojom::WebTestRunTestConfigurationPtr params =
        mojom::WebTestRunTestConfiguration::New();
    params->allow_external_pages = false;
    params->current_working_directory = current_working_directory_;
    params->temp_path = temp_path_;
    params->test_url = test_url_;
    params->allow_external_pages =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAllowExternalPages);
    params->expected_pixel_hash = expected_pixel_hash_;
    params->initial_size = initial_size_;
    params->protocol_mode = protocol_mode_;

    if (did_send_initial_test_configuration_) {
      GetWebTestRenderFrameRemote(frame)->ReplicateTestConfiguration(
          std::move(params));
    } else {
      did_send_initial_test_configuration_ = true;
      GetWebTestRenderFrameRemote(frame)->SetTestConfiguration(
          std::move(params));
      // Tests should always start with the browser controls hidden.
      frame->UpdateBrowserControlsState(BROWSER_CONTROLS_STATE_BOTH,
                                        BROWSER_CONTROLS_STATE_HIDDEN, false);
    }
  }

  // Is this a previously unknown renderer process_host?
  if (!render_process_host_observer_.IsObserving(process_host)) {
    render_process_host_observer_.Add(process_host);
    all_observed_render_process_hosts_.insert(process_host);

    if (!main_window)
      GetWebTestRenderFrameRemote(frame)
          ->SetupRendererProcessForNonTestWindow();

    GetWebTestRenderThreadRemote(process_host)
        ->ReplicateWebTestRuntimeFlagsChanges(
            accumulated_web_test_runtime_flags_changes_.Clone());
  }
}

void WebTestControlHost::OnTestFinished() {
  test_phase_ = CLEAN_UP;
  if (!printer_->output_finished())
    printer_->PrintImageFooter();
  if (main_window_)
    main_window_->web_contents()->ExitFullscreen(/*will_cause_resize=*/false);
  devtools_bindings_.reset();
  devtools_protocol_test_bindings_.reset();
  accumulated_web_test_runtime_flags_changes_.Clear();

  ShellBrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2, base::BindOnce(&WebTestControlHost::OnCleanupFinished,
                        weak_factory_.GetWeakPtr()));

  StoragePartition* storage_partition =
      BrowserContext::GetStoragePartition(browser_context, nullptr);
  storage_partition->GetServiceWorkerContext()->ClearAllServiceWorkersForTest(
      barrier_closure);
  storage_partition->ClearBluetoothAllowedDevicesMapForTesting();

  // TODO(nhiroki): Add a comment about the reason why we terminate all shared
  // workers here.
  TerminateAllSharedWorkers(
      BrowserContext::GetStoragePartition(
          ShellContentBrowserClient::Get()->browser_context(), nullptr),
      barrier_closure);
}

void WebTestControlHost::OnCleanupFinished() {
  if (secondary_window_) {
    secondary_window_->web_contents()->Stop();
    GetWebTestRenderFrameRemote(
        secondary_window_->web_contents()->GetRenderViewHost()->GetMainFrame())
        ->ResetRendererAfterWebTest();
    ++waiting_for_reset_done_;
  }
  if (main_window_) {
    main_window_->web_contents()->Stop();
    GetWebTestRenderFrameRemote(
        main_window_->web_contents()->GetRenderViewHost()->GetMainFrame())
        ->ResetRendererAfterWebTest();
    ++waiting_for_reset_done_;
  }
}

void WebTestControlHost::OnCaptureDumpCompleted(mojom::WebTestDumpPtr dump) {
  main_frame_dump_ = std::move(dump);

  waiting_for_main_frame_dump_ = false;
  ReportResults();
}

void WebTestControlHost::OnPixelDumpCaptured(const SkBitmap& snapshot) {
  DCHECK(!snapshot.drawsNothing());
  pixel_dump_ = snapshot;
  waiting_for_pixel_results_ = false;
  ReportResults();
}

void WebTestControlHost::ReportResults() {
  if (waiting_for_pixel_results_ || waiting_for_main_frame_dump_)
    return;
  if (main_frame_dump_->audio)
    OnAudioDump(*main_frame_dump_->audio);
  if (main_frame_dump_->layout)
    OnTextDump(*main_frame_dump_->layout);
  // If we have local pixels, report that. Otherwise report whatever the pixel
  // dump received from the renderer contains.
  if (pixel_dump_) {
    // See if we need to draw the selection bounds rect on top of the snapshot.
    if (!main_frame_dump_->selection_rect.IsEmpty())
      DrawSelectionRect(*pixel_dump_, main_frame_dump_->selection_rect);
    // The snapshot arrives from the GPU process via shared memory. Because MSan
    // can't track initializedness across processes, we must assure it that the
    // pixels are in fact initialized.
    MSAN_UNPOISON(pixel_dump_->getPixels(), pixel_dump_->computeByteSize());
    base::MD5Digest digest;
    base::MD5Sum(pixel_dump_->getPixels(), pixel_dump_->computeByteSize(),
                 &digest);
    actual_pixel_hash_ = base::MD5DigestToBase16(digest);

    OnImageDump(actual_pixel_hash_, *pixel_dump_);
  } else if (!main_frame_dump_->actual_pixel_hash.empty()) {
    OnImageDump(main_frame_dump_->actual_pixel_hash, main_frame_dump_->pixels);
  }
  OnTestFinished();
}

void WebTestControlHost::OnImageDump(const std::string& actual_pixel_hash,
                                     const SkBitmap& image) {
  printer_->PrintImageHeader(actual_pixel_hash, expected_pixel_hash_);

  // Only encode and dump the png if the hashes don't match. Encoding the
  // image is really expensive.
  if (actual_pixel_hash != expected_pixel_hash_) {
    std::vector<unsigned char> png;

    bool discard_transparency = true;
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kForceOverlayFullscreenVideo)) {
      discard_transparency = false;
    }

    gfx::PNGCodec::ColorFormat pixel_format;
    switch (image.info().colorType()) {
      case kBGRA_8888_SkColorType:
        pixel_format = gfx::PNGCodec::FORMAT_BGRA;
        break;
      case kRGBA_8888_SkColorType:
        pixel_format = gfx::PNGCodec::FORMAT_RGBA;
        break;
      default:
        NOTREACHED();
        return;
    }

    std::vector<gfx::PNGCodec::Comment> comments;
    comments.push_back(gfx::PNGCodec::Comment("checksum", actual_pixel_hash));
    bool success = gfx::PNGCodec::Encode(
        static_cast<const unsigned char*>(image.getPixels()), pixel_format,
        gfx::Size(image.width(), image.height()),
        static_cast<int>(image.rowBytes()), discard_transparency, comments,
        &png);
    if (success)
      printer_->PrintImageBlock(png);
  }
  printer_->PrintImageFooter();
}

void WebTestControlHost::OnAudioDump(const std::vector<unsigned char>& dump) {
  printer_->PrintAudioHeader();
  printer_->PrintAudioBlock(dump);
  printer_->PrintAudioFooter();
}

void WebTestControlHost::OnTextDump(const std::string& dump) {
  printer_->PrintTextHeader();
  printer_->PrintTextBlock(dump);
  if (!navigation_history_dump_.empty())
    printer_->PrintTextBlock(navigation_history_dump_);
  printer_->PrintTextFooter();
}

void WebTestControlHost::OnWebTestRuntimeFlagsChanged(
    int sender_process_host_id,
    const base::DictionaryValue& changed_web_test_runtime_flags) {
  // Stash the accumulated changes for future, not-yet-created renderers.
  accumulated_web_test_runtime_flags_changes_.MergeDictionary(
      &changed_web_test_runtime_flags);

  // Propagate the changes to all the tracked renderer processes.
  for (RenderProcessHost* process : all_observed_render_process_hosts_) {
    // Do not propagate the changes back to the process that originated
    // them. (propagating them back could also clobber subsequent changes in the
    // originator).
    if (process->GetID() == sender_process_host_id)
      continue;

    GetWebTestRenderThreadRemote(process)->ReplicateWebTestRuntimeFlagsChanges(
        changed_web_test_runtime_flags.Clone());
  }
}

void WebTestControlHost::OnDumpFrameLayoutResponse(int frame_tree_node_id,
                                                   const std::string& dump) {
  // Store the result.
  auto pair = frame_to_layout_dump_map_.insert(
      std::make_pair(frame_tree_node_id, dump));
  bool insertion_took_place = pair.second;
  DCHECK(insertion_took_place);

  // See if we need to wait for more responses.
  pending_layout_dumps_--;
  DCHECK_LE(0, pending_layout_dumps_);
  if (pending_layout_dumps_ > 0)
    return;

  // If the main test window was destroyed while waiting for the responses, then
  // there is nobody to receive the |stitched_layout_dump| and finish the test.
  if (!web_contents()) {
    OnTestFinished();
    return;
  }

  // Stitch the frame-specific results in the right order.
  std::string stitched_layout_dump;
  for (auto* render_frame_host : web_contents()->GetAllFrames()) {
    auto it =
        frame_to_layout_dump_map_.find(render_frame_host->GetFrameTreeNodeId());
    if (it != frame_to_layout_dump_map_.end()) {
      const std::string& dump = it->second;
      stitched_layout_dump.append(dump);
    }
  }

  // Continue finishing the test.
  GetWebTestRenderFrameRemote(
      main_window_->web_contents()->GetRenderViewHost()->GetMainFrame())
      ->LayoutDumpCompleted(stitched_layout_dump);
}

void WebTestControlHost::PrintMessageToStderr(const std::string& message) {
  printer_->AddMessageToStderr(message);
}

void WebTestControlHost::PrintMessage(const std::string& message) {
  printer_->AddMessageRaw(message);
}

void WebTestControlHost::OverridePreferences(const WebPreferences& prefs) {
  should_override_prefs_ = true;
  prefs_ = prefs;

  // Notifies the main RenderViewHost that Blink preferences changed so
  // immediately apply the new settings and to avoid re-usage of cached
  // preferences that are now stale. RenderViewHost::UpdateWebkitPreferences is
  // not used here because it would send an unneeded preferences update to the
  // renderer.
  RenderViewHost* main_render_view_host =
      main_window_->web_contents()->GetRenderViewHost();
  main_render_view_host->OnWebkitPreferencesChanged();
}

void WebTestControlHost::SetPopupBlockingEnabled(bool block_popups) {
  WebTestContentBrowserClient::Get()->SetPopupBlockingEnabled(block_popups);
}

void WebTestControlHost::SetScreenOrientationChanged() {
  WebTestContentBrowserClient::Get()->SetScreenOrientationChanged(true);
}

void WebTestControlHost::GetWritableDirectory(
    GetWritableDirectoryCallback reply_callback) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!writable_directory_for_tests_.IsValid()) {
    if (!writable_directory_for_tests_.CreateUniqueTempDir()) {
      LOG(ERROR) << "Failed to create temporary directory, test might not work "
                    "correctly";
    }
  }
  std::move(reply_callback).Run(writable_directory_for_tests_.GetPath());
}

namespace {

// A fake ui::SelectFileDialog, which will select a single pre-determined path.
class FakeSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeSelectFileDialog(base::FilePath result,
                       Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)),
        result_(std::move(result)) {}

 protected:
  ~FakeSelectFileDialog() override = default;

  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    listener_->FileSelected(result_, 0, params);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  base::FilePath result_;
};

class FakeSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit FakeSelectFileDialogFactory(base::FilePath result)
      : result_(std::move(result)) {}
  ~FakeSelectFileDialogFactory() override = default;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new FakeSelectFileDialog(result_, listener, std::move(policy));
  }

 private:
  base::FilePath result_;
};

}  // namespace

void WebTestControlHost::SetFilePathForMockFileDialog(
    const base::FilePath& path) {
  ui::SelectFileDialog::SetFactory(new FakeSelectFileDialogFactory(path));
}

void WebTestControlHost::FocusDevtoolsSecondaryWindow() {
  CHECK(secondary_window_);
  // We don't go down the normal system path of focusing RenderWidgetHostView
  // because on mac headless, there are no system windows and that path does
  // not do anything. Instead we go through the Shell::ActivateContents() path
  // which knows how to perform the activation correctly on all platforms and in
  // headless mode.
  secondary_window_->ActivateContents(secondary_window_->web_contents());
}

void WebTestControlHost::GoToOffset(int offset) {
  main_window_->GoBackOrForward(offset);
}

void WebTestControlHost::Reload() {
  main_window_->Reload();
}

void WebTestControlHost::LoadURLForFrame(const GURL& url,
                                         const std::string& frame_name) {
  main_window_->LoadURLForFrame(url, frame_name, ui::PAGE_TRANSITION_LINK);
}

void WebTestControlHost::CloseRemainingWindows() {
  DevToolsAgentHost::DetachAllClients();
  std::vector<Shell*> open_windows(Shell::windows());
  for (auto* shell : open_windows) {
    if (shell != main_window_ && shell != secondary_window_)
      shell->Close();
  }
  base::RunLoop().RunUntilIdle();
}

void WebTestControlHost::ResetRendererAfterWebTestDone() {
  if (--waiting_for_reset_done_ > 0)
    return;

  if (leak_detector_) {
    if (main_window_ && main_window_->web_contents()) {
      RenderViewHost* rvh = main_window_->web_contents()->GetRenderViewHost();
      leak_detector_->TryLeakDetection(
          rvh->GetProcess(),
          base::BindOnce(&WebTestControlHost::OnLeakDetectionDone,
                         weak_factory_.GetWeakPtr()));
    }
    return;
  }

  Shell::QuitMainMessageLoopForTesting();
}

void WebTestControlHost::OnLeakDetectionDone(
    const LeakDetector::LeakDetectionReport& report) {
  if (!report.leaked) {
    Shell::QuitMainMessageLoopForTesting();
    return;
  }

  printer_->AddErrorMessage(base::StringPrintf(
      "#LEAK - renderer pid %d (%s)", current_pid_, report.detail.c_str()));
  CHECK(!crash_when_leak_found_);

  DiscardMainWindow();
}

void WebTestControlHost::SetBluetoothManualChooser(bool enable) {
  bluetooth_chooser_factory_.reset();
  if (enable) {
    bluetooth_chooser_factory_.reset(new WebTestBluetoothChooserFactory());
  }
}

void WebTestControlHost::GetBluetoothManualChooserEvents() {
  if (!bluetooth_chooser_factory_) {
    printer_->AddErrorMessage(
        "FAIL: Must call setBluetoothManualChooser before "
        "getBluetoothManualChooserEvents.");
    return;
  }
  GetWebTestRenderFrameRemote(
      main_window_->web_contents()->GetRenderViewHost()->GetMainFrame())
      ->ReplyBluetoothManualChooserEvents(
          bluetooth_chooser_factory_->GetAndResetEvents());
}

void WebTestControlHost::SendBluetoothManualChooserEvent(
    const std::string& event_name,
    const std::string& argument) {
  if (!bluetooth_chooser_factory_) {
    printer_->AddErrorMessage(
        "FAIL: Must call setBluetoothManualChooser before "
        "sendBluetoothManualChooserEvent.");
    return;
  }
  BluetoothChooser::Event event;
  if (event_name == "cancelled") {
    event = BluetoothChooser::Event::CANCELLED;
  } else if (event_name == "selected") {
    event = BluetoothChooser::Event::SELECTED;
  } else if (event_name == "rescan") {
    event = BluetoothChooser::Event::RESCAN;
  } else {
    printer_->AddErrorMessage(base::StringPrintf(
        "FAIL: Unexpected sendBluetoothManualChooserEvent() event name '%s'.",
        event_name.c_str()));
    return;
  }
  bluetooth_chooser_factory_->SendEvent(event, argument);
}

void WebTestControlHost::BlockThirdPartyCookies(bool block) {
  ShellBrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  browser_context->GetDefaultStoragePartition(browser_context)
      ->GetCookieManagerForBrowserProcess()
      ->BlockThirdPartyCookies(block);
}

void WebTestControlHost::AddWebTestControlHostReceiver(
    mojo::PendingAssociatedReceiver<mojom::WebTestControlHost> receiver) {
  blink_test_client_receivers_.Add(this, std::move(receiver));
}

mojo::AssociatedRemote<mojom::WebTestRenderFrame>&
WebTestControlHost::GetWebTestRenderFrameRemote(RenderFrameHost* frame) {
  GlobalFrameRoutingId key(frame->GetProcess()->GetID(), frame->GetRoutingID());
  if (web_test_render_frame_map_.find(key) ==
      web_test_render_frame_map_.end()) {
    mojo::AssociatedRemote<mojom::WebTestRenderFrame>& new_ptr =
        web_test_render_frame_map_[key];
    frame->GetRemoteAssociatedInterfaces()->GetInterface(&new_ptr);
    new_ptr.set_disconnect_handler(
        base::BindOnce(&WebTestControlHost::HandleWebTestRenderFrameRemoteError,
                       weak_factory_.GetWeakPtr(), key));
  }
  DCHECK(web_test_render_frame_map_[key].get());
  return web_test_render_frame_map_[key];
}

mojo::AssociatedRemote<mojom::WebTestRenderThread>&
WebTestControlHost::GetWebTestRenderThreadRemote(RenderProcessHost* process) {
  if (web_test_render_thread_map_.find(process) ==
      web_test_render_thread_map_.end()) {
    IPC::ChannelProxy* channel = process->GetChannel();
    // channel might be null in tests.
    if (process->IsInitializedAndNotDead() && channel) {
      mojo::AssociatedRemote<mojom::WebTestRenderThread>& new_ptr =
          web_test_render_thread_map_[process];
      channel->GetRemoteAssociatedInterface(&new_ptr);
      new_ptr.set_disconnect_handler(base::BindOnce(
          &WebTestControlHost::HandleWebTestRenderThreadRemoteError,
          weak_factory_.GetWeakPtr(), process));
    }
  }
  DCHECK(web_test_render_thread_map_[process].get());
  return web_test_render_thread_map_[process];
}

void WebTestControlHost::HandleWebTestRenderFrameRemoteError(
    const GlobalFrameRoutingId& key) {
  web_test_render_frame_map_.erase(key);
}

void WebTestControlHost::HandleWebTestRenderThreadRemoteError(
    RenderProcessHost* key) {
  web_test_render_thread_map_.erase(key);
}

WebTestControlHost::Node::Node() = default;
WebTestControlHost::Node::Node(RenderFrameHost* host)
    : render_frame_host(host),
      render_frame_host_id(host->GetProcess()->GetID(), host->GetRoutingID()) {}
WebTestControlHost::Node::Node(Node&& other) = default;
WebTestControlHost::Node::~Node() = default;

}  // namespace content
