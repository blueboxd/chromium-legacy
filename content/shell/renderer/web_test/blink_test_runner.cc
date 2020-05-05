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
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/web_test/blink_test_helpers.h"
#include "content/shell/renderer/web_test/gamepad_controller.h"
#include "content/shell/renderer/web_test/pixel_dump.h"
#include "content/shell/renderer/web_test/test_interfaces.h"
#include "content/shell/renderer/web_test/test_runner.h"
#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"
#include "content/shell/renderer/web_test/web_view_test_proxy.h"
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
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebVector;
using blink::WebView;

namespace content {

BlinkTestRunner::BlinkTestRunner(WebViewTestProxy* web_view_test_proxy)
    : web_view_test_proxy_(web_view_test_proxy),
      test_config_(mojom::WebTestRunTestConfiguration::New()) {}

BlinkTestRunner::~BlinkTestRunner() = default;

void BlinkTestRunner::PrintMessageToStderr(const std::string& message) {
  GetWebTestControlHostRemote()->PrintMessageToStderr(message);
}

void BlinkTestRunner::PrintMessage(const std::string& message) {
  GetWebTestControlHostRemote()->PrintMessage(message);
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

void BlinkTestRunner::OverridePreferences(const WebPreferences& prefs) {
  GetWebTestControlHostRemote()->OverridePreferences(prefs);
}

void BlinkTestRunner::SetPopupBlockingEnabled(bool block_popups) {
  GetWebTestControlHostRemote()->SetPopupBlockingEnabled(block_popups);
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

void BlinkTestRunner::SetBluetoothFakeAdapter(const std::string& adapter_name,
                                              base::OnceClosure callback) {
  GetBluetoothFakeAdapterSetter().Set(adapter_name, std::move(callback));
}

void BlinkTestRunner::SetBluetoothManualChooser(bool enable) {
  GetWebTestControlHostRemote()->SetBluetoothManualChooser(enable);
}

void BlinkTestRunner::GetBluetoothManualChooserEvents(
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  get_bluetooth_events_callbacks_.push_back(std::move(callback));
  GetWebTestControlHostRemote()->GetBluetoothManualChooserEvents();
}

void BlinkTestRunner::SendBluetoothManualChooserEvent(
    const std::string& event,
    const std::string& argument) {
  GetWebTestControlHostRemote()->SendBluetoothManualChooserEvent(event,
                                                                 argument);
}

void BlinkTestRunner::SetBlockThirdPartyCookies(bool block) {
  GetWebTestControlHostRemote()->BlockThirdPartyCookies(block);
}

void BlinkTestRunner::SetLocale(const std::string& locale) {
  setlocale(LC_ALL, locale.c_str());
  // Number to string conversions require C locale, regardless of what
  // all the other subsystems are set to.
  setlocale(LC_NUMERIC, "C");
}

base::FilePath BlinkTestRunner::GetWritableDirectory() {
  base::FilePath result;
  GetWebTestControlHostRemote()->GetWritableDirectory(&result);
  return result;
}

void BlinkTestRunner::SetFilePathForMockFileDialog(const base::FilePath& path) {
  GetWebTestControlHostRemote()->SetFilePathForMockFileDialog(path);
}

void BlinkTestRunner::OnWebTestRuntimeFlagsChanged(
    const base::DictionaryValue& changed_values) {
  // Ignore changes that happen before we got the initial, accumulated
  // web flag changes in either OnReplicateTestConfiguration or
  // OnSetTestConfiguration.
  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();
  if (!interfaces->TestIsRunning())
    return;

  GetWebTestClientRemote()->WebTestRuntimeFlagsChanged(changed_values.Clone());
}

void BlinkTestRunner::TestFinished() {
  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();
  TestRunner* test_runner = interfaces->GetTestRunner();

  // We might get multiple TestFinished calls, ensure to only process the dump
  // once.
  if (!interfaces->TestIsRunning())
    return;
  interfaces->SetTestIsRunning(false);

  // If we're not in the main frame, then ask the browser to redirect the call
  // to the main frame instead.
  if (!is_main_window_ || !web_view_test_proxy_->GetMainRenderFrame()) {
    GetWebTestControlHostRemote()->TestFinishedInSecondaryRenderer();
    return;
  }

  // Now we know that we're in the main frame, we should generate dump results.
  // Clean out the lifecycle if needed before capturing the web tree
  // dump and pixels from the compositor.
  auto* web_frame =
      web_view_test_proxy_->GetWebView()->MainFrame()->ToWebLocalFrame();
  web_frame->FrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);

  // Initialize a new dump results object which we will populate in the calls
  // below.
  dump_result_ = mojom::WebTestDump::New();

  bool browser_should_dump_back_forward_list =
      test_runner->ShouldDumpBackForwardList();

  if (test_runner->ShouldDumpAsAudio()) {
    CaptureLocalAudioDump();

    GetWebTestControlHostRemote()->InitiateCaptureDump(
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
    GetWebTestControlHostRemote()->InitiateCaptureDump(
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
  GetWebTestControlHostRemote()->InitiateCaptureDump(
      browser_should_dump_back_forward_list,
      !test_runner->CanDumpPixelsFromRenderer());
}

void BlinkTestRunner::CaptureLocalAudioDump() {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalAudioDump");
  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();
  dump_result_->audio.emplace();
  interfaces->GetTestRunner()->GetAudioData(&*dump_result_->audio);
}

void BlinkTestRunner::CaptureLocalLayoutDump() {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalLayoutDump");
  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();
  TestRunner* test_runner = interfaces->GetTestRunner();
  std::string layout;
  if (test_runner->HasCustomTextDump(&layout)) {
    dump_result_->layout.emplace(layout + "\n");
  } else if (!test_runner->IsRecursiveLayoutDumpRequested()) {
    dump_result_->layout.emplace(test_runner->DumpLayout(
        web_view_test_proxy_->GetMainRenderFrame()->GetWebFrame()));
  } else {
    // TODO(vmpstr): Since CaptureDump is called from the browser, we can be
    // smart and move this logic directly to the browser.
    waiting_for_layout_dump_results_ = true;
    GetWebTestControlHostRemote()->InitiateLayoutDump();
  }
}

void BlinkTestRunner::CaptureLocalPixelsDump() {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalPixelsDump");

  // Test finish should only be processed in the BlinkTestRunner associated
  // with the current, non-swapped-out RenderView.
  DCHECK(web_view_test_proxy_->GetWebView()->MainFrame()->IsWebLocalFrame());

  waiting_for_pixels_dump_result_ = true;

  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();
  interfaces->GetTestRunner()->DumpPixelsAsync(
      web_view_test_proxy_,
      base::BindOnce(&BlinkTestRunner::OnPixelsDumpCompleted,
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
  GetWebTestControlHostRemote()->CloseRemainingWindows();
}

void BlinkTestRunner::DeleteAllCookies() {
  GetWebTestClientRemote()->DeleteAllCookies();
}

int BlinkTestRunner::NavigationEntryCount() {
  return web_view_test_proxy_->GetLocalSessionHistoryLengthForTesting();
}

void BlinkTestRunner::GoToOffset(int offset) {
  GetWebTestControlHostRemote()->GoToOffset(offset);
}

void BlinkTestRunner::Reload() {
  GetWebTestControlHostRemote()->Reload();
}

void BlinkTestRunner::LoadURLForFrame(const WebURL& url,
                                      const std::string& frame_name) {
  GetWebTestControlHostRemote()->LoadURLForFrame(url, frame_name);
}

bool BlinkTestRunner::AllowExternalPages() {
  return test_config_->allow_external_pages;
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

void BlinkTestRunner::SetScreenOrientationChanged() {
  GetWebTestControlHostRemote()->SetScreenOrientationChanged();
}

void BlinkTestRunner::FocusDevtoolsSecondaryWindow() {
  GetWebTestControlHostRemote()->FocusDevtoolsSecondaryWindow();
}

void BlinkTestRunner::SetTrustTokenKeyCommitments(
    const std::string& raw_commitments,
    base::OnceClosure callback) {
  GetWebTestClientRemote()->SetTrustTokenKeyCommitments(raw_commitments,
                                                        std::move(callback));
}

void BlinkTestRunner::ClearTrustTokenState(base::OnceClosure callback) {
  GetWebTestClientRemote()->ClearTrustTokenState(std::move(callback));
}

void BlinkTestRunner::CaptureDump(
    mojom::WebTestRenderFrame::CaptureDumpCallback callback) {
  // TODO(vmpstr): This is only called on the main frame. One suggestion is to
  // split the interface on which this call lives so that it is only accessible
  // to the main frame (as opposed to all frames).
  DCHECK(is_main_window_ && web_view_test_proxy_->GetMainRenderFrame());

  dump_callback_ = std::move(callback);
  CaptureDumpComplete();
}

void BlinkTestRunner::DidCommitNavigationInMainFrame() {
  // This method is just meant to catch the about:blank navigation started in
  // ResetRendererAfterWebTest().
  if (!waiting_for_reset_navigation_to_about_blank_)
    return;

  WebFrame* main_frame = web_view_test_proxy_->GetWebView()->MainFrame();
  DCHECK(main_frame->IsWebLocalFrame());

  // This would mean some other navigation was already happening when the test
  // ended, the about:blank should still be coming.
  GURL url = main_frame->ToWebLocalFrame()->GetDocumentLoader()->GetUrl();
  if (!url.IsAboutBlank())
    return;

  waiting_for_reset_navigation_to_about_blank_ = false;
  GetWebTestControlHostRemote()->ResetRendererAfterWebTestDone();
}

mojom::WebTestBluetoothFakeAdapterSetter&
BlinkTestRunner::GetBluetoothFakeAdapterSetter() {
  if (!bluetooth_fake_adapter_setter_) {
    RenderThread::Get()->BindHostReceiver(
        bluetooth_fake_adapter_setter_.BindNewPipeAndPassReceiver());
  }
  return *bluetooth_fake_adapter_setter_;
}

mojo::AssociatedRemote<mojom::WebTestControlHost>&
BlinkTestRunner::GetWebTestControlHostRemote() {
  if (!web_test_control_host_remote_) {
    RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
        &web_test_control_host_remote_);
    web_test_control_host_remote_.set_disconnect_handler(
        base::BindOnce(&BlinkTestRunner::HandleWebTestControlHostDisconnected,
                       base::Unretained(this)));
  }
  return web_test_control_host_remote_;
}

void BlinkTestRunner::HandleWebTestControlHostDisconnected() {
  web_test_control_host_remote_.reset();
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

void BlinkTestRunner::OnSetupRendererProcessForNonTestWindow() {
  DCHECK(!is_main_window_);

  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();
  // Allows the window to receive replicated WebTestRuntimeFlags and to
  // control or end the test.
  interfaces->SetTestIsRunning(true);

  // All non-main windows get sized to 800x600 (so does the main window).
  // This is done for the main frame only, not child local roots in other
  // processes.
  if (web_view_test_proxy_->GetMainRenderFrame()) {
    RenderWidget* widget =
        web_view_test_proxy_->GetMainRenderFrame()->GetLocalRootRenderWidget();
    gfx::Rect window_rect(widget->WindowRect().x, widget->WindowRect().y, 800,
                          600);
    widget->SetWindowRectSynchronouslyForTesting(window_rect);
  }
}

void BlinkTestRunner::ApplyTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr params) {
  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();

  test_config_ = params.Clone();

  is_main_window_ = true;
  interfaces->SetMainView(web_view_test_proxy_->GetWebView());

  interfaces->SetTestIsRunning(true);
  interfaces->ConfigureForTestWithURL(params->test_url, params->protocol_mode);
}

void BlinkTestRunner::OnReplicateTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr params) {
  ApplyTestConfiguration(std::move(params));
}

void BlinkTestRunner::OnSetTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr params) {
  DCHECK(web_view_test_proxy_->GetMainRenderFrame());

  gfx::Size window_size = params->initial_size;

  ApplyTestConfiguration(std::move(params));

  RenderWidget* widget =
      web_view_test_proxy_->GetMainRenderFrame()->GetLocalRootRenderWidget();
  gfx::Rect window_rect(widget->WindowRect().x, widget->WindowRect().y,
                        window_size.width(), window_size.height());
  widget->SetWindowRectSynchronouslyForTesting(window_rect);
}

void BlinkTestRunner::OnResetRendererAfterWebTest() {
  // BlinkTestMsg_Reset should always be sent to the *current* view.
  DCHECK(web_view_test_proxy_->GetMainRenderFrame());

  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();
  interfaces->ResetAll();

  // Navigating to about:blank will make sure that no new loads are initiated
  // by the renderer.
  waiting_for_reset_navigation_to_about_blank_ = true;

  blink::WebURLRequest request{GURL(url::kAboutBlankURL)};
  request.SetMode(network::mojom::RequestMode::kNavigate);
  request.SetRedirectMode(network::mojom::RedirectMode::kManual);
  request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);
  request.SetRequestorOrigin(blink::WebSecurityOrigin::CreateUniqueOpaque());
  web_view_test_proxy_->GetMainRenderFrame()->GetWebFrame()->StartNavigation(
      request);
}

void BlinkTestRunner::OnFinishTestInMainWindow() {
  DCHECK(is_main_window_ && web_view_test_proxy_->GetMainRenderFrame());

  // Avoid a situation where TestFinished is called twice, because
  // of a racey test finish in 2 secondary renderers.
  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();
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

}  // namespace content
