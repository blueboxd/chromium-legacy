// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_LACROS_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_LACROS_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace crosapi {
struct Bitmap;
}  // namespace crosapi

namespace content {

// This class is responsible for communicating with ash-chrome to get snapshots
// of the desktop. This class is used on several different threads with no clear
// signaling. This is a contextual requirement of the current implementation of
// the media capture code. We do our best to:
//   * Minimize state stored in this class.
//   * Ensure that stored state is accessed safely.
class DesktopCapturerLacros : public webrtc::DesktopCapturer {
 public:
  enum CaptureType { kScreen, kWindow };
  DesktopCapturerLacros(CaptureType capture_type,
                        const webrtc::DesktopCaptureOptions& options);
  DesktopCapturerLacros(const DesktopCapturerLacros&) = delete;
  DesktopCapturerLacros& operator=(const DesktopCapturerLacros&) = delete;
  ~DesktopCapturerLacros() override;

  // DesktopCapturer:
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  bool FocusOnSelectedSource() override;
  bool IsOccluded(const webrtc::DesktopVector& pos) override;
  void SetSharedMemoryFactory(std::unique_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void SetExcludedWindow(webrtc::WindowId window) override;

 private:
  // Callback for when ash-chrome returns a snapshot of the screen or window as
  // a bitmap.
  void DidTakeSnapshot(bool success, const crosapi::Bitmap& snapshot);

  // Whether this object is capturing screens or windows.
  const CaptureType capture_type_;

  // TODO(https://crbug.com/1094460): The webrtc options for screen/display
  // capture are currently ignored.
  const webrtc::DesktopCaptureOptions options_;

  // For window capture, this is the source that we want to capture.
  SourceId selected_source_;

  // The webrtc::DesktopCapturer interface expects the implementation to hold
  // onto and call a Callback* object. This instance relies on the assumption
  // that Callback* will outlive this instance.
  //
  // The current media capture implementation expects that the implementation of
  // CaptureFrame() synchronously invokes |callback_| in a re-entrant fashion.
  // Thus, we do not worry about thread safety when invoking callback_.
  Callback* callback_ = nullptr;

  // This remote is thread safe. Callbacks are invoked on the calling sequence.
  mojo::SharedRemote<crosapi::mojom::ScreenManager> screen_manager_;

  base::WeakPtrFactory<DesktopCapturerLacros> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_LACROS_H_
