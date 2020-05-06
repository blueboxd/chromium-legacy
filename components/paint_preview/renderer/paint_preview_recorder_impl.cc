// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_impl.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/metrics/histogram_functions.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_recorder.h"
#include "components/paint_preview/renderer/paint_preview_recorder_utils.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace paint_preview {

namespace {

mojom::PaintPreviewStatus FinishRecording(
    sk_sp<const cc::PaintRecord> recording,
    const gfx::Rect& bounds,
    PaintPreviewTracker* tracker,
    base::File skp_file,
    size_t max_capture_size,
    mojom::PaintPreviewCaptureResponse* response) {
  TRACE_EVENT0("paint_preview", "FinishRecording");
  DCHECK(response);
  DCHECK(tracker);
  if (!response || !tracker)
    return mojom::PaintPreviewStatus::kCaptureFailed;

  TRACE_EVENT_BEGIN0("paint_preview", "ParseGlyphs");
  ParseGlyphs(recording.get(), tracker);
  TRACE_EVENT_END0("paint_preview", "ParseGlyphs");
  size_t serialized_size = 0;
  if (!SerializeAsSkPicture(recording, tracker, bounds, std::move(skp_file),
                            max_capture_size, &serialized_size))
    return mojom::PaintPreviewStatus::kCaptureFailed;

  BuildResponse(tracker, response);
  response->serialized_size = serialized_size;
  return mojom::PaintPreviewStatus::kOk;
}

}  // namespace

PaintPreviewRecorderImpl::PaintPreviewRecorderImpl(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      is_painting_preview_(false),
      is_main_frame_(render_frame->IsMainFrame()) {
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&PaintPreviewRecorderImpl::BindPaintPreviewRecorder,
                          weak_ptr_factory_.GetWeakPtr()));
}

PaintPreviewRecorderImpl::~PaintPreviewRecorderImpl() = default;

void PaintPreviewRecorderImpl::CapturePaintPreview(
    mojom::PaintPreviewCaptureParamsPtr params,
    CapturePaintPreviewCallback callback) {
  TRACE_EVENT0("paint_preview",
               "PaintPreviewRecorderImpl::CapturePaintPreview");
  mojom::PaintPreviewStatus status = mojom::PaintPreviewStatus::kOk;
  base::ReadOnlySharedMemoryRegion region;
  // This should not be called recursively or multiple times while unfinished
  // (Blink can only run one capture per RenderFrame at a time).
  DCHECK(!is_painting_preview_);
  // DCHECK, but fallback safely as it is difficult to reason about whether this
  // might happen due to it being tied to a RenderFrame rather than
  // RenderWidget and we don't want to crash the renderer as this is
  // recoverable.
  auto response = mojom::PaintPreviewCaptureResponse::New();
  if (is_painting_preview_) {
    status = mojom::PaintPreviewStatus::kAlreadyCapturing;
    std::move(callback).Run(status, std::move(response));
    return;
  }
  base::AutoReset<bool>(&is_painting_preview_, true);

  CapturePaintPreviewInternal(params, response.get(), &status);
  std::move(callback).Run(status, std::move(response));
}

void PaintPreviewRecorderImpl::OnDestruct() {
  paint_preview_recorder_receiver_.reset();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void PaintPreviewRecorderImpl::BindPaintPreviewRecorder(
    mojo::PendingAssociatedReceiver<mojom::PaintPreviewRecorder> receiver) {
  paint_preview_recorder_receiver_.Bind(std::move(receiver));
}

void PaintPreviewRecorderImpl::CapturePaintPreviewInternal(
    const mojom::PaintPreviewCaptureParamsPtr& params,
    mojom::PaintPreviewCaptureResponse* response,
    mojom::PaintPreviewStatus* status) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Ensure the a frame actually exists to avoid a possible crash.
  if (!frame) {
    DVLOG(1) << "Error: renderer has no frame yet!";
    return;
  }

  // Warm up paint for an out-of-lifecycle paint phase.
  frame->DispatchBeforePrintEvent();

  DCHECK_EQ(is_main_frame_, params->is_main_frame);
  gfx::Rect bounds;
  if (is_main_frame_ || params->clip_rect == gfx::Rect(0, 0, 0, 0)) {
    auto size = frame->DocumentSize();

    // |size| may be 0 if a tab is captured prior to layout finishing. This
    // shouldn't occur often, if at all, in normal usage. However, this may
    // occur during tests. Capturing prior to layout is non-sensical as the
    // canvas size cannot be deremined so just abort.
    if (size.height == 0 || size.width == 0) {
      *status = mojom::PaintPreviewStatus::kCaptureFailed;
      return;
    }
    bounds = gfx::Rect(0, 0, size.width, size.height);
  } else {
    bounds = gfx::Rect(params->clip_rect.size());
  }

  cc::PaintRecorder recorder;
  PaintPreviewTracker tracker(params->guid, frame->GetEmbeddingToken(),
                              is_main_frame_);
  cc::PaintCanvas* canvas =
      recorder.beginRecording(bounds.width(), bounds.height());
  canvas->SetPaintPreviewTracker(&tracker);

  // Use time ticks manually rather than a histogram macro so as to;
  // 1. Account for main frames and subframes separately.
  // 2. Mitigate binary size as this won't be used that often.
  // 3. Record only on successes as failures are likely to be outliers (fast or
  //    slow).
  base::TimeTicks start_time = base::TimeTicks::Now();
  TRACE_EVENT_BEGIN0("paint_preview", "WebLocalFrame::CapturePaintPreview");
  bool success = frame->CapturePaintPreview(bounds, canvas);
  TRACE_EVENT_END0("paint_preview", "WebLocalFrame::CapturePaintPreview");
  base::TimeDelta capture_time = base::TimeTicks::Now() - start_time;
  response->blink_recording_time = capture_time;

  if (is_main_frame_) {
    base::UmaHistogramBoolean("Renderer.PaintPreview.Capture.MainFrameSuccess",
                              success);
    if (success) {
      // Main frame should generally be the largest cost and will always run so
      // it is tracked separately.
      base::UmaHistogramTimes(
          "Renderer.PaintPreview.Capture.MainFrameBlinkCaptureDuration",
          capture_time);
    }
  } else {
    base::UmaHistogramBoolean("Renderer.PaintPreview.Capture.SubframeSuccess",
                              success);
    if (success) {
      base::UmaHistogramTimes(
          "Renderer.PaintPreview.Capture.SubframeBlinkCaptureDuration",
          capture_time);
    }
  }

  // Restore to before out-of-lifecycle paint phase.
  frame->DispatchAfterPrintEvent();
  if (!success) {
    *status = mojom::PaintPreviewStatus::kCaptureFailed;
    return;
  }

  // TODO(crbug/1011896): Determine if making this async would be beneficial.
  *status = FinishRecording(recorder.finishRecordingAsPicture(), bounds,
                            &tracker, std::move(params->file),
                            params->max_capture_size, response);
}

}  // namespace paint_preview
