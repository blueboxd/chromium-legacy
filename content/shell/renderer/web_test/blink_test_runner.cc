// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/blink_test_runner.h"

#include <stddef.h>

#include <algorithm>
#include <clocale>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/hash/md5.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/plugins/renderer/plugin_placeholder.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/test/web_test_support_renderer.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/web_test/blink_test_helpers.h"
#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"
#include "content/shell/test_runner/app_banner_service.h"
#include "content/shell/test_runner/gamepad_controller.h"
#include "content/shell/test_runner/pixel_dump.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "ipc/ipc_sync_channel.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/capture/video_capturer_source.h"
#include "media/media_buildflags.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/app_banner/app_banner.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_context_menu_data.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/icc_profile.h"

using blink::Platform;
using blink::WebContextMenuData;
using blink::WebElement;
using blink::WebFrame;
using blink::WebHistoryItem;
using blink::WebLocalFrame;
using blink::WebRect;
using blink::WebScriptSource;
using blink::WebSize;
using blink::WebString;
using blink::WebTestingSupport;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebVector;
using blink::WebView;

namespace content {

namespace {

class UseSynchronousResizeModeVisitor : public RenderViewVisitor {
 public:
  explicit UseSynchronousResizeModeVisitor(bool enable) : enable_(enable) {}
  ~UseSynchronousResizeModeVisitor() override {}

  bool Visit(RenderView* render_view) override {
    UseSynchronousResizeMode(render_view, enable_);
    return true;
  }

 private:
  bool enable_;
};

class MockVideoCapturerSource : public media::VideoCapturerSource {
 public:
  MockVideoCapturerSource() = default;
  ~MockVideoCapturerSource() override {}

  media::VideoCaptureFormats GetPreferredFormats() override {
    const int supported_width = 640;
    const int supported_height = 480;
    const float supported_framerate = 60.0;
    return media::VideoCaptureFormats(
        1, media::VideoCaptureFormat(
               gfx::Size(supported_width, supported_height),
               supported_framerate, media::PIXEL_FORMAT_I420));
  }
  void StartCapture(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& new_frame_callback,
                    const RunningCallback& running_callback) override {
    running_callback.Run(true);
  }
  void StopCapture() override {}
};

class MockAudioCapturerSource : public media::AudioCapturerSource {
 public:
  MockAudioCapturerSource() = default;

  void Initialize(const media::AudioParameters& params,
                  CaptureCallback* callback) override {}
  void Start() override {}
  void Stop() override {}
  void SetVolume(double volume) override {}
  void SetAutomaticGainControl(bool enable) override {}
  void SetOutputDeviceForAec(const std::string& output_device_id) override {}

 protected:
  ~MockAudioCapturerSource() override {}
};

}  // namespace

BlinkTestRunner::BlinkTestRunner(RenderView* render_view)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<BlinkTestRunner>(render_view),
      test_config_(mojom::ShellTestConfiguration::New()) {}

BlinkTestRunner::~BlinkTestRunner() {}

// WebTestDelegate  -----------------------------------------------------------

void BlinkTestRunner::ClearEditCommand() {
  render_view()->ClearEditCommands();
}

void BlinkTestRunner::SetEditCommand(const std::string& name,
                                     const std::string& value) {
  render_view()->SetEditCommandForNextKeyEvent(name, value);
}

void BlinkTestRunner::PrintMessageToStderr(const std::string& message) {
  GetBlinkTestClientRemote()->PrintMessageToStderr(message);
}

void BlinkTestRunner::PrintMessage(const std::string& message) {
  if (!is_secondary_window_)
    GetBlinkTestClientRemote()->PrintMessage(message);
}

void BlinkTestRunner::PostTask(base::OnceClosure task) {
  GetTaskRunner()->PostTask(FROM_HERE, std::move(task));
}

void BlinkTestRunner::PostDelayedTask(base::OnceClosure task,
                                      base::TimeDelta delay) {
  GetTaskRunner()->PostDelayedTask(FROM_HERE, std::move(task), delay);
}

WebString BlinkTestRunner::RegisterIsolatedFileSystem(
    const blink::WebVector<blink::WebString>& absolute_filenames) {
  std::vector<base::FilePath> files;
  for (auto& filename : absolute_filenames)
    files.push_back(blink::WebStringToFilePath(filename));
  std::string filesystem_id;
  GetWebTestClientRemote()->RegisterIsolatedFileSystem(files, &filesystem_id);
  return WebString::FromUTF8(filesystem_id);
}

long long BlinkTestRunner::GetCurrentTimeInMillisecond() {
  return base::TimeDelta(base::Time::Now() - base::Time::UnixEpoch())
             .ToInternalValue() /
         base::Time::kMicrosecondsPerMillisecond;
}

WebString BlinkTestRunner::GetAbsoluteWebStringFromUTF8Path(
    const std::string& utf8_path) {
  base::FilePath path = base::FilePath::FromUTF8Unsafe(utf8_path);
  if (!path.IsAbsolute()) {
    GURL base_url =
        net::FilePathToFileURL(test_config_->current_working_directory.Append(
            FILE_PATH_LITERAL("foo")));
    net::FileURLToFilePath(base_url.Resolve(utf8_path), &path);
  }
  return blink::FilePathToWebString(path);
}

WebURL BlinkTestRunner::RewriteWebTestsURL(const std::string& utf8_url,
                                           bool is_wpt_mode) {
  return content::RewriteWebTestsURL(utf8_url, is_wpt_mode);
}

test_runner::TestPreferences* BlinkTestRunner::Preferences() {
  return &prefs_;
}

void BlinkTestRunner::ApplyPreferences() {
  WebPreferences prefs = render_view()->GetWebkitPreferences();
  ExportWebTestSpecificPreferences(prefs_, &prefs);
  render_view()->SetWebkitPreferences(prefs);
  GetBlinkTestClientRemote()->OverridePreferences(prefs);
}

void BlinkTestRunner::SetPopupBlockingEnabled(bool block_popups) {
  GetBlinkTestClientRemote()->SetPopupBlockingEnabled(block_popups);
}

void BlinkTestRunner::UseUnfortunateSynchronousResizeMode(bool enable) {
  UseSynchronousResizeModeVisitor visitor(enable);
  RenderView::ForEach(&visitor);
}

void BlinkTestRunner::EnableAutoResizeMode(const WebSize& min_size,
                                           const WebSize& max_size) {
  content::EnableAutoResizeMode(render_view(), min_size, max_size);
}

void BlinkTestRunner::DisableAutoResizeMode(const WebSize& new_size) {
  content::DisableAutoResizeMode(render_view(), new_size);
  ForceResizeRenderView(render_view(), new_size);
}

void BlinkTestRunner::ResetAutoResizeMode() {
  // An empty size indicates to keep the size as is. Resetting races with the
  // browser setting up the new test (one is a mojo IPC (OnSetTestConfiguration)
  // and one is legacy (OnReset)) so we can not clobber the size here.
  content::DisableAutoResizeMode(render_view(), gfx::Size());
  // Does not call ForceResizeRenderView() here intentionally. This is between
  // tests, and the next test will set up a size.
}

void BlinkTestRunner::NavigateSecondaryWindow(const GURL& url) {
  GetBlinkTestClientRemote()->NavigateSecondaryWindow(url);
}

void BlinkTestRunner::InspectSecondaryWindow() {
  GetWebTestClientRemote()->InspectSecondaryWindow();
}

void BlinkTestRunner::ClearAllDatabases() {
  GetWebTestClientRemote()->ClearAllDatabases();
}

void BlinkTestRunner::SetDatabaseQuota(int quota) {
  GetWebTestClientRemote()->SetDatabaseQuota(quota);
}

void BlinkTestRunner::SimulateWebNotificationClick(
    const std::string& title,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply) {
  GetWebTestClientRemote()->SimulateWebNotificationClick(
      title, action_index.value_or(std::numeric_limits<int32_t>::min()), reply);
}

void BlinkTestRunner::SimulateWebNotificationClose(const std::string& title,
                                                   bool by_user) {
  GetWebTestClientRemote()->SimulateWebNotificationClose(title, by_user);
}

void BlinkTestRunner::SimulateWebContentIndexDelete(const std::string& id) {
  GetWebTestClientRemote()->SimulateWebContentIndexDelete(id);
}

void BlinkTestRunner::SetDeviceScaleFactor(float factor) {
  content::SetDeviceScaleFactor(render_view(), factor);
}

std::unique_ptr<blink::WebInputEvent>
BlinkTestRunner::TransformScreenToWidgetCoordinates(
    test_runner::WebWidgetTestProxy* web_widget_test_proxy,
    const blink::WebInputEvent& event) {
  return content::TransformScreenToWidgetCoordinates(web_widget_test_proxy,
                                                     event);
}

test_runner::WebWidgetTestProxy* BlinkTestRunner::GetWebWidgetTestProxy(
    blink::WebLocalFrame* frame) {
  return content::GetWebWidgetTestProxy(frame);
}

void BlinkTestRunner::EnableUseZoomForDSF() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableUseZoomForDSF);
}

bool BlinkTestRunner::IsUseZoomForDSFEnabled() {
  return content::IsUseZoomForDSFEnabled();
}

void BlinkTestRunner::SetDeviceColorSpace(const std::string& name) {
  content::SetDeviceColorSpace(render_view(), GetTestingColorSpace(name));
}

void BlinkTestRunner::SetBluetoothFakeAdapter(const std::string& adapter_name,
                                              base::OnceClosure callback) {
  GetBluetoothFakeAdapterSetter().Set(adapter_name, std::move(callback));
}

void BlinkTestRunner::SetBluetoothManualChooser(bool enable) {
  GetBlinkTestClientRemote()->SetBluetoothManualChooser(enable);
}

void BlinkTestRunner::GetBluetoothManualChooserEvents(
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  get_bluetooth_events_callbacks_.push_back(std::move(callback));
  GetBlinkTestClientRemote()->GetBluetoothManualChooserEvents();
}

void BlinkTestRunner::SendBluetoothManualChooserEvent(
    const std::string& event,
    const std::string& argument) {
  GetBlinkTestClientRemote()->SendBluetoothManualChooserEvent(event, argument);
}

void BlinkTestRunner::SetFocus(blink::WebView* web_view, bool focus) {
  RenderView* render_view = RenderView::FromWebView(web_view);
  if (render_view)  // Check whether |web_view| has been already closed.
    SetFocusAndActivate(render_view, focus);
}

void BlinkTestRunner::SetBlockThirdPartyCookies(bool block) {
  GetWebTestClientRemote()->BlockThirdPartyCookies(block);
}

std::string BlinkTestRunner::PathToLocalResource(const std::string& resource) {
#if defined(OS_WIN)
  if (base::StartsWith(resource, "/tmp/", base::CompareCase::SENSITIVE)) {
    // We want a temp file.
    GURL base_url = net::FilePathToFileURL(test_config_->temp_path);
    return base_url.Resolve(resource.substr(sizeof("/tmp/") - 1)).spec();
  }
#endif

  // Some web tests use file://// which we resolve as a UNC path. Normalize
  // them to just file:///.
  std::string result = resource;
  static const size_t kFileLen = sizeof("file:///") - 1;
  while (base::StartsWith(base::ToLowerASCII(result), "file:////",
                          base::CompareCase::SENSITIVE)) {
    result = result.substr(0, kFileLen) + result.substr(kFileLen + 1);
  }
  return RewriteWebTestsURL(result, false /* is_wpt_mode */).GetString().Utf8();
}

void BlinkTestRunner::SetLocale(const std::string& locale) {
  setlocale(LC_ALL, locale.c_str());
  // Number to string conversions require C locale, regardless of what
  // all the other subsystems are set to.
  setlocale(LC_NUMERIC, "C");
}

base::FilePath BlinkTestRunner::GetWritableDirectory() {
  base::FilePath result;
  GetWebTestClientRemote()->GetWritableDirectory(&result);
  return result;
}

void BlinkTestRunner::SetFilePathForMockFileDialog(const base::FilePath& path) {
  GetWebTestClientRemote()->SetFilePathForMockFileDialog(path);
}

void BlinkTestRunner::OnWebTestRuntimeFlagsChanged(
    const base::DictionaryValue& changed_values) {
  // Ignore changes that happen before we got the initial, accumulated
  // web flag changes in either OnReplicateTestConfiguration or
  // OnSetTestConfiguration.
  test_runner::TestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  if (!interfaces->TestIsRunning())
    return;

  GetWebTestClientRemote()->WebTestRuntimeFlagsChanged(changed_values.Clone());
}

void BlinkTestRunner::TestFinished() {
  test_runner::TestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  test_runner::TestRunner* test_runner = interfaces->GetTestRunner();

  // We might get multiple TestFinished calls, ensure to only process the dump
  // once.
  if (!interfaces->TestIsRunning())
    return;
  interfaces->SetTestIsRunning(false);

  // If we're not in the main frame, then ask the browser to redirect the call
  // to the main frame instead.
  if (!is_main_window_ || !render_view()->GetMainRenderFrame()) {
    GetWebTestClientRemote()->TestFinishedInSecondaryRenderer();
    return;
  }

  // Now we know that we're in the main frame, we should generate dump results.
  // Clean out the lifecycle if needed before capturing the web tree
  // dump and pixels from the compositor.
  auto* web_frame = render_view()->GetWebView()->MainFrame()->ToWebLocalFrame();
  web_frame->FrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);

  // Initialize a new dump results object which we will populate in the calls
  // below.
  dump_result_ = mojom::BlinkTestDump::New();

  bool browser_should_dump_back_forward_list =
      test_runner->ShouldDumpBackForwardList();

  if (test_runner->ShouldDumpAsAudio()) {
    CaptureLocalAudioDump();

    GetWebTestClientRemote()->InitiateCaptureDump(
        browser_should_dump_back_forward_list,
        /*browser_should_capture_pixels=*/false);
    return;
  }

  // TODO(vmpstr): Sometimes the web isn't stable, which means that if we
  // just ask the browser to ask us to do a dump, the layout would be
  // different compared to if we do it now. This probably needs to be
  // rebaselined. But for now, just capture a local web first.
  CaptureLocalLayoutDump();

  if (!test_runner->ShouldGeneratePixelResults()) {
    GetWebTestClientRemote()->InitiateCaptureDump(
        browser_should_dump_back_forward_list,
        /*browser_should_capture_pixels=*/false);
    return;
  }

  if (test_runner->CanDumpPixelsFromRenderer()) {
    // This does the capture in the renderer when possible, otherwise
    // we will ask the browser to initiate it.
    CaptureLocalPixelsDump();
  } else {
    // If the browser should capture pixels, then we shouldn't be waiting
    // for layout dump results. Any test can only require the browser to
    // dump one or the other at this time.
    DCHECK(!waiting_for_layout_dump_results_);
    if (test_runner->ShouldDumpSelectionRect()) {
      dump_result_->selection_rect =
          web_frame->GetSelectionBoundsRectForTesting();
    }
  }
  GetWebTestClientRemote()->InitiateCaptureDump(
      browser_should_dump_back_forward_list,
      !test_runner->CanDumpPixelsFromRenderer());
}

void BlinkTestRunner::CaptureLocalAudioDump() {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalAudioDump");
  test_runner::TestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  dump_result_->audio.emplace();
  interfaces->GetTestRunner()->GetAudioData(&*dump_result_->audio);
}

void BlinkTestRunner::CaptureLocalLayoutDump() {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalLayoutDump");
  test_runner::TestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  test_runner::TestRunner* test_runner = interfaces->GetTestRunner();
  std::string layout;
  if (test_runner->HasCustomTextDump(&layout)) {
    dump_result_->layout.emplace(layout + "\n");
  } else if (!test_runner->IsRecursiveLayoutDumpRequested()) {
    dump_result_->layout.emplace(test_runner->DumpLayout(
        render_view()->GetMainRenderFrame()->GetWebFrame()));
  } else {
    // TODO(vmpstr): Since CaptureDump is called from the browser, we can be
    // smart and move this logic directly to the browser.
    waiting_for_layout_dump_results_ = true;
    GetBlinkTestClientRemote()->InitiateLayoutDump();
  }
}

void BlinkTestRunner::CaptureLocalPixelsDump() {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalPixelsDump");

  // Test finish should only be processed in the BlinkTestRunner associated
  // with the current, non-swapped-out RenderView.
  DCHECK(render_view()->GetWebView()->MainFrame()->IsWebLocalFrame());

  waiting_for_pixels_dump_result_ = true;

  test_runner::TestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  interfaces->GetTestRunner()->DumpPixelsAsync(
      render_view(), base::BindOnce(&BlinkTestRunner::OnPixelsDumpCompleted,
                                    base::Unretained(this)));
}

void BlinkTestRunner::OnLayoutDumpCompleted(std::string completed_layout_dump) {
  CHECK(waiting_for_layout_dump_results_);
  dump_result_->layout.emplace(completed_layout_dump);
  waiting_for_layout_dump_results_ = false;
  CaptureDumpComplete();
}

void BlinkTestRunner::OnPixelsDumpCompleted(const SkBitmap& snapshot) {
  CHECK(waiting_for_pixels_dump_result_);
  DCHECK_NE(0, snapshot.info().width());
  DCHECK_NE(0, snapshot.info().height());

  // The snapshot arrives from the GPU process via shared memory. Because MSan
  // can't track initializedness across processes, we must assure it that the
  // pixels are in fact initialized.
  MSAN_UNPOISON(snapshot.getPixels(), snapshot.computeByteSize());
  base::MD5Digest digest;
  base::MD5Sum(snapshot.getPixels(), snapshot.computeByteSize(), &digest);
  std::string actual_pixel_hash = base::MD5DigestToBase16(digest);

  dump_result_->actual_pixel_hash = actual_pixel_hash;
  if (actual_pixel_hash != test_config_->expected_pixel_hash)
    dump_result_->pixels = snapshot;

  waiting_for_pixels_dump_result_ = false;
  CaptureDumpComplete();
}

void BlinkTestRunner::CaptureDumpComplete() {
  // Abort if we're still waiting for some results.
  if (waiting_for_layout_dump_results_ || waiting_for_pixels_dump_result_)
    return;

  // Abort if the browser didn't ask us for the dump yet.
  if (!dump_callback_)
    return;

  std::move(dump_callback_).Run(std::move(dump_result_));
}

void BlinkTestRunner::CloseRemainingWindows() {
  GetBlinkTestClientRemote()->CloseRemainingWindows();
}

void BlinkTestRunner::DeleteAllCookies() {
  GetWebTestClientRemote()->DeleteAllCookies();
}

int BlinkTestRunner::NavigationEntryCount() {
  return GetLocalSessionHistoryLength(render_view());
}

void BlinkTestRunner::GoToOffset(int offset) {
  GetBlinkTestClientRemote()->GoToOffset(offset);
}

void BlinkTestRunner::Reload() {
  GetBlinkTestClientRemote()->Reload();
}

void BlinkTestRunner::LoadURLForFrame(const WebURL& url,
                                      const std::string& frame_name) {
  GetBlinkTestClientRemote()->LoadURLForFrame(url, frame_name);
}

bool BlinkTestRunner::AllowExternalPages() {
  return test_config_->allow_external_pages;
}

void BlinkTestRunner::FetchManifest(
    blink::WebView* view,
    base::OnceCallback<void(const blink::WebURL&, const blink::Manifest&)>
        callback) {
  ::content::FetchManifest(view, std::move(callback));
}

void BlinkTestRunner::SetPermission(const std::string& name,
                                    const std::string& value,
                                    const GURL& origin,
                                    const GURL& embedding_origin) {
  GetWebTestClientRemote()->SetPermission(
      name, blink::ToPermissionStatus(value), origin, embedding_origin);
}

void BlinkTestRunner::ResetPermissions() {
  GetWebTestClientRemote()->ResetPermissions();
}

void BlinkTestRunner::DispatchBeforeInstallPromptEvent(
    const std::vector<std::string>& event_platforms,
    base::OnceCallback<void(bool)> callback) {
  app_banner_service_.reset(new test_runner::AppBannerService());
  render_view()->GetMainRenderFrame()->BindLocalInterface(
      blink::mojom::AppBannerController::Name_,
      app_banner_service_->controller()
          .BindNewPipeAndPassReceiver()
          .PassPipe());
  app_banner_service_->SendBannerPromptRequest(event_platforms,
                                               std::move(callback));
}

void BlinkTestRunner::ResolveBeforeInstallPromptPromise(
    const std::string& platform) {
  if (app_banner_service_) {
    app_banner_service_->ResolvePromise(platform);
    app_banner_service_.reset(nullptr);
  }
}

blink::WebPlugin* BlinkTestRunner::CreatePluginPlaceholder(
    const blink::WebPluginParams& params) {
  if (params.mime_type != "application/x-plugin-placeholder-test")
    return nullptr;

  plugins::PluginPlaceholder* placeholder = new plugins::PluginPlaceholder(
      render_view()->GetMainRenderFrame(), params, "<div>Test content</div>");
  return placeholder->plugin();
}

void BlinkTestRunner::RunIdleTasks(base::OnceClosure callback) {
  SchedulerRunIdleTasks(std::move(callback));
}

void BlinkTestRunner::ForceTextInputStateUpdate(WebLocalFrame* frame) {
  ForceTextInputStateUpdateForRenderFrame(RenderFrame::FromWebFrame(frame));
}

void BlinkTestRunner::SetScreenOrientationChanged() {
  GetBlinkTestClientRemote()->SetScreenOrientationChanged();
}

// RenderViewObserver  --------------------------------------------------------

void BlinkTestRunner::DidClearWindowObject(WebLocalFrame* frame) {
  WebTestingSupport::InjectInternalsObject(frame);
}

// Public methods - -----------------------------------------------------------

void BlinkTestRunner::Reset(bool for_new_test) {
  prefs_.Reset();
  waiting_for_reset_ = false;

  WebFrame* main_frame = render_view()->GetWebView()->MainFrame();

  render_view()->ClearEditCommands();
  if (for_new_test) {
    if (main_frame->IsWebLocalFrame()) {
      WebLocalFrame* local_main_frame = main_frame->ToWebLocalFrame();
      local_main_frame->SetName(WebString());
      GetWebWidgetTestProxy(local_main_frame)->EndSyntheticGestures();
    }
    main_frame->ClearOpener();
  }

  // Resetting the internals object also overrides the WebPreferences, so we
  // have to sync them to WebKit again.
  if (main_frame->IsWebLocalFrame()) {
    WebTestingSupport::ResetInternalsObject(main_frame->ToWebLocalFrame());
    render_view()->SetWebkitPreferences(render_view()->GetWebkitPreferences());
  }
}

void BlinkTestRunner::CaptureDump(
    mojom::BlinkTestControl::CaptureDumpCallback callback) {
  // TODO(vmpstr): This is only called on the main frame. One suggestion is to
  // split the interface on which this call lives so that it is only accessible
  // to the main frame (as opposed to all frames).
  DCHECK(is_main_window_ && render_view()->GetMainRenderFrame());

  dump_callback_ = std::move(callback);
  CaptureDumpComplete();
}

void BlinkTestRunner::DidCommitNavigationInMainFrame() {
  WebFrame* main_frame = render_view()->GetWebView()->MainFrame();
  if (!waiting_for_reset_ || !main_frame->IsWebLocalFrame())
    return;
  GURL url = main_frame->ToWebLocalFrame()->GetDocumentLoader()->GetUrl();
  if (!url.IsAboutBlank())
    return;

  // Avoid a situation where ResetDone is called twice, because
  // ResetDone should be called once if a secondary renderer exists.
  if (is_secondary_window_)
    return;

  waiting_for_reset_ = false;

  GetBlinkTestClientRemote()->ResetDone();
}

// Private methods  -----------------------------------------------------------

mojom::WebTestBluetoothFakeAdapterSetter&
BlinkTestRunner::GetBluetoothFakeAdapterSetter() {
  if (!bluetooth_fake_adapter_setter_) {
    RenderThread::Get()->BindHostReceiver(
        bluetooth_fake_adapter_setter_.BindNewPipeAndPassReceiver());
  }
  return *bluetooth_fake_adapter_setter_;
}

mojo::AssociatedRemote<mojom::BlinkTestClient>&
BlinkTestRunner::GetBlinkTestClientRemote() {
  if (!blink_test_client_remote_) {
    RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
        &blink_test_client_remote_);
    blink_test_client_remote_.set_disconnect_handler(
        base::BindOnce(&BlinkTestRunner::HandleBlinkTestClientDisconnected,
                       base::Unretained(this)));
  }
  return blink_test_client_remote_;
}

void BlinkTestRunner::HandleBlinkTestClientDisconnected() {
  blink_test_client_remote_.reset();
}

mojo::AssociatedRemote<mojom::WebTestClient>&
BlinkTestRunner::GetWebTestClientRemote() {
  if (!web_test_client_remote_) {
    RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
        &web_test_client_remote_);
    web_test_client_remote_.set_disconnect_handler(
        base::BindOnce(&BlinkTestRunner::HandleWebTestClientDisconnected,
                       base::Unretained(this)));
  }
  return web_test_client_remote_;
}

void BlinkTestRunner::HandleWebTestClientDisconnected() {
  web_test_client_remote_.reset();
}

void BlinkTestRunner::OnSetupSecondaryRenderer() {
  DCHECK(!is_main_window_);

  // TODO(https://crbug.com/1039247): Consider to use |is_main_window_| instead
  // of |is_secondary_window_|. But, there are many test failures when using
  // |!is_main_window_|.
  is_secondary_window_ = true;

  test_runner::TestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  interfaces->SetTestIsRunning(true);
  ForceResizeRenderView(render_view(), WebSize(800, 600));
}

void BlinkTestRunner::ApplyTestConfiguration(
    mojom::ShellTestConfigurationPtr params) {
  test_runner::TestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();

  test_config_ = params.Clone();

  is_main_window_ = true;
  interfaces->SetMainView(render_view()->GetWebView());

  interfaces->SetTestIsRunning(true);
  interfaces->ConfigureForTestWithURL(params->test_url, params->protocol_mode);
}

void BlinkTestRunner::OnReplicateTestConfiguration(
    mojom::ShellTestConfigurationPtr params) {
  ApplyTestConfiguration(std::move(params));
}

void BlinkTestRunner::OnSetTestConfiguration(
    mojom::ShellTestConfigurationPtr params) {
  mojom::ShellTestConfigurationPtr local_params = params.Clone();
  ApplyTestConfiguration(std::move(params));

  ForceResizeRenderView(render_view(),
                        WebSize(local_params->initial_size.width(),
                                local_params->initial_size.height()));

  test_runner::TestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  test_runner::TestRunner* test_runner = interfaces->GetTestRunner();
  test_runner->SetFocus(render_view()->GetWebView(), true);
}

void BlinkTestRunner::OnReset() {
  // BlinkTestMsg_Reset should always be sent to the *current* view.
  DCHECK(render_view()->GetWebView()->MainFrame()->IsWebLocalFrame());
  WebLocalFrame* main_frame =
      render_view()->GetWebView()->MainFrame()->ToWebLocalFrame();

  WebTestRenderThreadObserver::GetInstance()->test_interfaces()->ResetAll();
  Reset(true /* for_new_test */);
  // Navigating to about:blank will make sure that no new loads are initiated
  // by the renderer.
  waiting_for_reset_ = true;

  blink::WebURLRequest request{GURL(url::kAboutBlankURL)};
  request.SetMode(network::mojom::RequestMode::kNavigate);
  request.SetRedirectMode(network::mojom::RedirectMode::kManual);
  request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);
  request.SetRequestorOrigin(blink::WebSecurityOrigin::CreateUniqueOpaque());
  main_frame->StartNavigation(request);
}

void BlinkTestRunner::OnTestFinishedInSecondaryRenderer() {
  DCHECK(is_main_window_ && render_view()->GetMainRenderFrame());

  // Avoid a situation where TestFinished is called twice, because
  // of a racey test finish in 2 secondary renderers.
  test_runner::TestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  if (!interfaces->TestIsRunning())
    return;

  TestFinished();
}

void BlinkTestRunner::OnReplyBluetoothManualChooserEvents(
    const std::vector<std::string>& events) {
  DCHECK(!get_bluetooth_events_callbacks_.empty());
  base::OnceCallback<void(const std::vector<std::string>&)> callback =
      std::move(get_bluetooth_events_callbacks_.front());
  get_bluetooth_events_callbacks_.pop_front();
  std::move(callback).Run(events);
}

void BlinkTestRunner::OnDestruct() {
  delete this;
}

scoped_refptr<base::SingleThreadTaskRunner> BlinkTestRunner::GetTaskRunner() {
  if (render_view()->GetWebView()->MainFrame()->IsWebLocalFrame()) {
    WebLocalFrame* main_frame =
        render_view()->GetWebView()->MainFrame()->ToWebLocalFrame();
    return main_frame->GetTaskRunner(blink::TaskType::kInternalTest);
  }
  return blink::scheduler::GetSingleThreadTaskRunnerForTesting();
}

}  // namespace content
