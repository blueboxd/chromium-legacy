// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"

#include <memory>

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/layout/layout_file_upload_control.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// Calculate metrics candidate every 1 second after the first text pre-paint.
constexpr TimeDelta kTimerDelay = TimeDelta::FromSeconds(1);
constexpr size_t kTextNodeNumberLimit = 5000;

bool LargeTextFirst(const base::WeakPtr<TextRecord>& a,
                    const base::WeakPtr<TextRecord>& b) {
  DCHECK(a);
  DCHECK(b);
  if (a->first_size != b->first_size)
    return a->first_size > b->first_size;
  // This make sure that two different nodes with the same |first_size| wouldn't
  // be merged in the set.
  return a->node_id > b->node_id;
}

}  // namespace

TextPaintTimingDetector::TextPaintTimingDetector(LocalFrameView* frame_view)
    : records_manager_(),
      timer_(frame_view->GetFrame().GetTaskRunner(TaskType::kInternalDefault),
             this,
             &TextPaintTimingDetector::TimerFired),
      frame_view_(frame_view) {}

void TextPaintTimingDetector::PopulateTraceValue(
    TracedValue& value,
    const TextRecord& first_text_paint) {
  value.SetInteger("DOMNodeId", static_cast<int>(first_text_paint.node_id));
#ifndef NDEBUG
  value.SetString("text", first_text_paint.text);
#endif
  value.SetInteger("size", static_cast<int>(first_text_paint.first_size));
  value.SetInteger("candidateIndex", ++count_candidates_);
  value.SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value.SetBoolean("isOOPIF",
                   !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
}

void TextPaintTimingDetector::ReportCandidateToTrace(
    const TextRecord& largest_text_record) {
  auto value = std::make_unique<TracedValue>();
  PopulateTraceValue(*value, largest_text_record);
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading", "LargestTextPaint::Candidate",
                                   largest_text_record.paint_time, "data",
                                   std::move(value), "frame",
                                   ToTraceValue(&frame_view_->GetFrame()));
}

void TextPaintTimingDetector::ReportNoCandidateToTrace() {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("candidateIndex", ++count_candidates_);
  value->SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value->SetBoolean("isOOPIF",
                    !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
  TRACE_EVENT2("loading", "LargestTextPaint::NoCandidate", "data",
               std::move(value), "frame",
               ToTraceValue(&frame_view_->GetFrame()));
}

void TextPaintTimingDetector::TimerFired(TimerBase* time) {
  // Wrap |UpdateCandidate| method in TimerFired so that we can drop |time| for
  // |UpdateCandidate| in testing.
  UpdateCandidate();
}

void TextPaintTimingDetector::UpdateCandidate() {
  if (!records_manager_.IsRecordingLargestTextPaint())
    return;

  DCHECK(RuntimeEnabledFeatures::FirstContentfulPaintPlusPlusEnabled());
  TextRecord* largest_text_record =
      records_manager_.FindLargestPaintCandidate();
  const base::TimeTicks time =
      largest_text_record ? largest_text_record->paint_time : base::TimeTicks();
  const uint64_t size =
      largest_text_record ? largest_text_record->first_size : 0;
  bool changed =
      frame_view_->GetPaintTimingDetector().NotifyIfChangedLargestTextPaint(
          time, size);
  if (!changed)
    return;

  if (largest_text_record && !largest_text_record->paint_time.is_null())
    ReportCandidateToTrace(*largest_text_record);
  else
    ReportNoCandidateToTrace();
}

void TextPaintTimingDetector::OnPaintFinished() {
  if (need_update_timing_at_frame_end_) {
    need_update_timing_at_frame_end_ = false;
    UpdateCandidate();
  }
  if (records_manager_.NeedMeausuringPaintTime()) {
    // Start repeating timer only once at the first text paint.
    if (!timer_.IsActive() && is_recording_ltp_)
      timer_.StartRepeating(kTimerDelay, FROM_HERE);
    if (!awaiting_swap_promise_) {
      RegisterNotifySwapTime(
          CrossThreadBindOnce(&TextPaintTimingDetector::ReportSwapTime,
                              WrapCrossThreadWeakPersistent(this)));
    }
  }
  // Delete unneeded information from |records_manager_| if no longer recording
  // LargestTextPaint.
  if (!is_recording_ltp_ && records_manager_.IsRecordingLargestTextPaint())
    records_manager_.StopRecordingLargestTextPaint();
}

void TextPaintTimingDetector::NotifyNodeRemoved(DOMNodeId node_id) {
  if (!is_recording_)
    return;
  if (records_manager_.IsKnownVisibleNode(node_id)) {
    records_manager_.RemoveVisibleRecord(node_id);
    need_update_timing_at_frame_end_ = true;
  } else if (records_manager_.IsKnownInvisibleNode(node_id)) {
    records_manager_.RemoveInvisibleRecord(node_id);
    need_update_timing_at_frame_end_ = true;
  }
}

void TextPaintTimingDetector::RegisterNotifySwapTime(
    ReportTimeCallback callback) {
  // ReportSwapTime on layerTreeView will queue a swap-promise, the callback is
  // called when the swap for current render frame completes or fails to happen.
  LocalFrame& frame = frame_view_->GetFrame();
  if (!frame.GetPage())
    return;
  frame.GetPage()->GetChromeClient().NotifySwapTime(frame, std::move(callback));
  awaiting_swap_promise_ = true;
}

void TextPaintTimingDetector::ReportSwapTime(WebWidgetClient::SwapResult result,
                                             base::TimeTicks timestamp) {
  if (!records_manager_.HasTextElementTiming()) {
    Document* document = frame_view_->GetFrame().GetDocument();
    if (document && RuntimeEnabledFeatures::ElementTimingEnabled(document)) {
      LocalDOMWindow* window = document->domWindow();
      if (window) {
        records_manager_.SetTextElementTiming(
            &TextElementTiming::From(*window));
      }
    }
  }
  records_manager_.AssignPaintTimeToQueuedNodes(timestamp);
  awaiting_swap_promise_ = false;
}

bool TextPaintTimingDetector::ShouldWalkObject(
    const LayoutBoxModelObject& object) const {
  if (!is_recording_)
    return false;
  // TODO(crbug.com/933479): Use LayoutObject::GeneratingNode() to include
  // anonymous objects' rect.
  Node* node = object.GetNode();
  if (!node)
    return false;
  // If we have finished recording Largest Text Paint and the element is a
  // shadow element or has no elementtiming attribute, then we should not record
  // its text.
  if (!is_recording_ltp_ && !TextElementTiming::NeededForElementTiming(node))
    return false;

  DOMNodeId node_id = DOMNodeIds::ExistingIdForNode(node);
  if (node_id == kInvalidDOMNodeId)
    return true;
  // This metric defines the size of a text block by its first size, so we
  // should not walk the object if it has been recorded.
  return !records_manager_.HasRecorded(node_id);
}

void TextPaintTimingDetector::RecordAggregatedText(
    const LayoutBoxModelObject& aggregator,
    const IntRect& aggregated_visual_rect,
    const PropertyTreeState& property_tree_state) {
  DCHECK(ShouldWalkObject(aggregator));
  DCHECK(!records_manager_.HasTooManyNodes());

  Node* node = aggregator.GetNode();
  DCHECK(node);
  DOMNodeId node_id = DOMNodeIds::IdForNode(node);
  DCHECK_NE(node_id, kInvalidDOMNodeId);

  // The caller should check this.
  DCHECK(!aggregated_visual_rect.IsEmpty());

  FloatRect mapped_visual_rect =
      frame_view_->GetPaintTimingDetector().CalculateVisualRect(
          aggregated_visual_rect, property_tree_state);
  uint64_t aggregated_size = mapped_visual_rect.Size().Area();

  if (aggregated_size == 0) {
    records_manager_.RecordInvisibleNode(node_id);
  } else {
    records_manager_.RecordVisibleNode(
        node_id, aggregated_size,
        TextElementTiming::ComputeIntersectionRect(
            node, aggregated_visual_rect, property_tree_state, frame_view_),
        aggregator);
  }

  if (records_manager_.HasTooManyNodes()) {
    TRACE_EVENT_INSTANT2("loading", "TextPaintTimingDetector::OverNodeLimit",
                         TRACE_EVENT_SCOPE_THREAD, "count_size_non_zero_nodes",
                         records_manager_.CountVisibleNodes(),
                         "count_size_zero_nodes",
                         records_manager_.CountInvisibleNodes());
    StopRecordEntries();
  }
}

TextRecord* TextPaintTimingDetector::FindLargestPaintCandidate() {
  return records_manager_.FindLargestPaintCandidate();
}

void TextPaintTimingDetector::StopRecordEntries() {
  timer_.Stop();
  is_recording_ = false;
}

void TextPaintTimingDetector::StopRecordingLargestTextPaint() {
  // This does not immediately call TextRecordsManager's
  // StopRecordingLargestTextPaint(). However, it stops the timer, and the next
  // time OnPaintFinished() is called, the TextRecordsManager has the last
  // chance to update LargestTextPaint (based on elements that had already been
  // painted), and THEN we call its StopRecordingLargestTextPaint() so that the
  // LargestTextPaint-only information is cleared.
  timer_.Stop();
  is_recording_ltp_ = false;
}

void TextPaintTimingDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(records_manager_);
  visitor->Trace(frame_view_);
}

TextRecordsManager::TextRecordsManager() : size_ordered_set_(&LargeTextFirst) {}

void TextRecordsManager::RemoveVisibleRecord(const DOMNodeId& node_id) {
  DCHECK(visible_node_map_.Contains(node_id));
  size_ordered_set_.erase(visible_node_map_.at(node_id)->AsWeakPtr());
  visible_node_map_.erase(node_id);
  is_result_invalidated_ = true;
}

void TextRecordsManager::RemoveInvisibleRecord(const DOMNodeId& node_id) {
  DCHECK(invisible_node_ids_.Contains(node_id));
  invisible_node_ids_.erase(node_id);
  is_result_invalidated_ = true;
}

void TextRecordsManager::AssignPaintTimeToQueuedNodes(
    const base::TimeTicks& timestamp) {
  // If texts_queued_for_paint_time_.size == 0, it means the array has been
  // consumed in a callback earlier than this one. That violates the assumption
  // that only one or zero callback will be called after one OnPaintFinished.
  DCHECK_GT(texts_queued_for_paint_time_.size(), 0UL);
  for (auto iterator = texts_queued_for_paint_time_.begin();
       iterator != texts_queued_for_paint_time_.end(); ++iterator) {
    // The record may have been removed between the callback registration and
    // invoking.
    base::WeakPtr<TextRecord>& record = *iterator;
    if (!record) {
      texts_queued_for_paint_time_.erase(iterator);
      continue;
    }
    DCHECK(visible_node_map_.Contains(record->node_id));
    DCHECK_EQ(record->paint_time, base::TimeTicks());
    record->paint_time = timestamp;
  }
  if (text_element_timing_)
    text_element_timing_->OnTextNodesPainted(texts_queued_for_paint_time_);
  texts_queued_for_paint_time_.clear();
  is_result_invalidated_ = true;
}

void TextRecordsManager::RecordVisibleNode(const DOMNodeId& node_id,
                                           const uint64_t& visual_size,
                                           const FloatRect& element_timing_rect,
                                           const LayoutObject& text_object) {
  DCHECK(!HasTooManyNodes());
  DCHECK_GT(visual_size, 0u);
  std::unique_ptr<TextRecord> record =
      std::make_unique<TextRecord>(node_id, visual_size, element_timing_rect);
#ifndef NDEBUG
  String text;
  if (text_object.IsText()) {
    text = ToLayoutText(&text_object)->GetText();
  } else if (text_object.IsFileUploadControl()) {
    text = ToLayoutFileUploadControl(&text_object)->FileTextValue();
  } else {
    text = String("NON-TEXT-OBJECT");
  }
  record->text = text;
#endif
  if (is_recording_ltp_)
    size_ordered_set_.emplace(record->AsWeakPtr());
  QueueToMeasurePaintTime(record->AsWeakPtr());
  visible_node_map_.insert(node_id, std::move(record));
  is_result_invalidated_ = true;
}

bool TextRecordsManager::HasTooManyNodes() const {
  return visible_node_map_.size() + invisible_node_ids_.size() >=
         kTextNodeNumberLimit;
}

TextRecord* TextRecordsManager::FindLargestPaintCandidate() {
  DCHECK(is_recording_ltp_);
  DCHECK_EQ(visible_node_map_.size(), size_ordered_set_.size());
  if (!is_result_invalidated_)
    return cached_largest_paint_candidate_;
  TextRecord* new_largest_paint_candidate = nullptr;
  for (auto it = size_ordered_set_.begin(); it != size_ordered_set_.end();
       ++it) {
    // WeakPtr::IsValid() is expensive. We use raw pointer to reduce the checks.
    TextRecord* text_record = (*it).get();
    DCHECK(text_record);
    if (text_record->paint_time.is_null())
      continue;
    DCHECK(visible_node_map_.Contains(text_record->node_id));
    new_largest_paint_candidate = text_record;
    break;
  }
  cached_largest_paint_candidate_ = new_largest_paint_candidate;
  is_result_invalidated_ = false;
  return new_largest_paint_candidate;
}

void TextRecordsManager::StopRecordingLargestTextPaint() {
  is_recording_ltp_ = false;
  size_ordered_set_.clear();
  cached_largest_paint_candidate_ = nullptr;
}

void TextRecordsManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(text_element_timing_);
}

}  // namespace blink
