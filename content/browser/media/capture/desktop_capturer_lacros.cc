// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capturer_lacros.h"

#include "base/debug/stack_trace.h"
#include "base/logging.h"

#include "chromeos/lacros/browser/lacros_chrome_service_impl.h"
#include "chromeos/lacros/cpp/window_snapshot.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

DesktopCapturerLacros::DesktopCapturerLacros(
    CaptureType capture_type,
    const webrtc::DesktopCaptureOptions& options)
    : capture_type_(capture_type), options_(options) {
  mojo::PendingRemote<lacros::mojom::ScreenManager> pending_screen_manager;
  mojo::PendingReceiver<lacros::mojom::ScreenManager> pending_receiver =
      pending_screen_manager.InitWithNewPipeAndPassReceiver();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DesktopCapturerLacros::BindReceiverMainThread,
                                std::move(pending_receiver)));

  // We create a SharedRemote that binds the underlying Remote onto a
  // dedicated sequence.
  screen_manager_ = mojo::SharedRemote<lacros::mojom::ScreenManager>(
      std::move(pending_screen_manager),
      base::ThreadPool::CreateSequencedTaskRunner({}));
}

DesktopCapturerLacros::~DesktopCapturerLacros() = default;

bool DesktopCapturerLacros::GetSourceList(SourceList* sources) {
  if (capture_type_ == kScreen) {
    // TODO(https://crbug.com/1094460): Implement this source list
    // appropriately.
    Source w;
    w.id = 1;
    sources->push_back(w);
    return true;
  }

  std::vector<lacros::mojom::WindowDetailsPtr> windows;
  {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    screen_manager_->ListWindows(&windows);
  }

  for (auto& window : windows) {
    Source w;
    w.id = window->id;
    w.title = window->title;
    sources->push_back(w);
  }
  return true;
}

bool DesktopCapturerLacros::SelectSource(SourceId id) {
  selected_source_ = id;
  return true;
}

bool DesktopCapturerLacros::FocusOnSelectedSource() {
  return true;
}

void DesktopCapturerLacros::Start(Callback* callback) {
  callback_ = callback;
}

void DesktopCapturerLacros::CaptureFrame() {
  if (capture_type_ == kScreen) {
    lacros::WindowSnapshot snapshot;
    {
      // lacros-chrome is allowed to make sync calls to ash-chrome.
      mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
      screen_manager_->TakeScreenSnapshot(&snapshot);
    }
    DidTakeSnapshot(/*success=*/true, snapshot);
  } else {
    bool success;
    lacros::WindowSnapshot snapshot;
    {
      // lacros-chrome is allowed to make sync calls to ash-chrome.
      mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
      screen_manager_->TakeWindowSnapshot(selected_source_, &success,
                                          &snapshot);
    }
    DidTakeSnapshot(success, snapshot);
  }
}

bool DesktopCapturerLacros::IsOccluded(const webrtc::DesktopVector& pos) {
  return false;
}

void DesktopCapturerLacros::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {}

void DesktopCapturerLacros::SetExcludedWindow(webrtc::WindowId window) {}

// static
void DesktopCapturerLacros::BindReceiverMainThread(
    mojo::PendingReceiver<lacros::mojom::ScreenManager> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The lacros chrome service must exist at all points in time for the lacros
  // browser.
  auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
  DCHECK(lacros_chrome_service);

  lacros_chrome_service->BindScreenManagerReceiver(std::move(receiver));
}

void DesktopCapturerLacros::DidTakeSnapshot(
    bool success,
    const lacros::WindowSnapshot& snapshot) {
  if (!success) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT,
                               std::unique_ptr<webrtc::DesktopFrame>());
    return;
  }

  std::unique_ptr<webrtc::DesktopFrame> frame =
      std::make_unique<webrtc::BasicDesktopFrame>(
          webrtc::DesktopSize(snapshot.width, snapshot.height));

  // This code assumes that the stride is 4 * width. This relies on the
  // assumption that there's no padding and each pixel is 4 bytes.
  frame->CopyPixelsFrom(
      snapshot.bitmap.data(), 4 * snapshot.width,
      webrtc::DesktopRect::MakeWH(snapshot.width, snapshot.height));

  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
}

}  // namespace content
