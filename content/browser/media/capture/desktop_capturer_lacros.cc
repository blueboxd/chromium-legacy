// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capturer_lacros.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

#include "chromeos/crosapi/cpp/bitmap.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace content {

DesktopCapturerLacros::DesktopCapturerLacros(
    CaptureType capture_type,
    const webrtc::DesktopCaptureOptions& options)
    : capture_type_(capture_type), options_(options) {
  mojo::PendingRemote<crosapi::mojom::ScreenManager> pending_screen_manager;
  mojo::PendingReceiver<crosapi::mojom::ScreenManager> pending_receiver =
      pending_screen_manager.InitWithNewPipeAndPassReceiver();

  // The lacros chrome service exists at all times except during early start-up
  // and late shut-down. This class should never be used in those two times.
  auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
  DCHECK(lacros_chrome_service);
  lacros_chrome_service->BindScreenManagerReceiver(std::move(pending_receiver));

  // We create a SharedRemote that binds the underlying Remote onto a
  // dedicated sequence.
  screen_manager_ = mojo::SharedRemote<crosapi::mojom::ScreenManager>(
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

  std::vector<crosapi::mojom::WindowDetailsPtr> windows;
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
    crosapi::Bitmap snapshot;
    {
      // lacros-chrome is allowed to make sync calls to ash-chrome.
      mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
      screen_manager_->TakeScreenSnapshot(&snapshot);
    }
    DidTakeSnapshot(/*success=*/true, snapshot);
  } else {
    bool success;
    crosapi::Bitmap snapshot;
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

void DesktopCapturerLacros::DidTakeSnapshot(bool success,
                                            const crosapi::Bitmap& snapshot) {
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
      snapshot.pixels.data(), 4 * snapshot.width,
      webrtc::DesktopRect::MakeWH(snapshot.width, snapshot.height));

  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
}

}  // namespace content
