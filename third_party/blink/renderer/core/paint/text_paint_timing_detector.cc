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
#include "third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.h"
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
constexpr base::TimeDelta kTimerDelay = base::TimeDelta::FromSeconds(1);
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
  // TODO(crbug.com/976893): Remove DOMNodeId.
  value.SetInteger("DOMNodeId", static_cast<int>(first_text_paint.node_id));
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
  // TODO(crbug.com/976894): Check if the event is needed before preparing the
  // trace value.
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

// The timer has guaranteed that |this| exists when this function is invoked.
void TextPaintTimingDetector::TimerFired(TimerBase* time) {
  // Wrap |UpdateCandidate| method in TimerFired so that we can drop |time| for
  // |UpdateCandidate| in testing.
  UpdateCandidate();
}

void TextPaintTimingDetector::UpdateCandidate() {
  if (!is_recording_)
    return;
  if (!is_recording_ltp_)
    return;

  DCHECK(RuntimeEnabledFeatures::FirstContentfulPaintPlusPlusEnabled());
  base::WeakPtr<TextRecord> largest_text_record =
      records_manager_.FindLargestPaintCandidate();
  const base::TimeTicks time =
      largest_text_record ? largest_text_record->paint_time : base::TimeTicks();
  const uint64_t size =
      largest_text_record ? largest_text_record->first_size : 0;
  PaintTimingDetector& detector = frame_view_->GetPaintTimingDetector();
  bool changed = detector.NotifyIfChangedLargestTextPaint(time, size);
  if (!changed)
    return;

  if (largest_text_record && !largest_text_record->paint_time.is_null()) {
    if (auto* lcp_calculator = detector.GetLargestContentfulPaintCalculator())
      lcp_calculator->OnLargestTextUpdated(largest_text_record);
    ReportCandidateToTrace(*largest_text_record);
  } else {
    if (auto* lcp_calculator = detector.GetLargestContentfulPaintCalculator())
      lcp_calculator->OnLargestTextUpdated(nullptr);
    ReportNoCandidateToTrace();
  }
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
      // |WrapCrossThreadWeakPersistent| guarantees that when |this| is killed,
      // the callback function will not be invoked.
      RegisterNotifySwapTime(
          CrossThreadBindOnce(&TextPaintTimingDetector::ReportSwapTime,
                              WrapCrossThreadWeakPersistent(this)));
    }
  }
}

void TextPaintTimingDetector::LayoutObjectWillBeDestroyed(
    const LayoutObject& object) {
  if (!is_recording_)
    return;
  if (records_manager_.IsKnownVisible(object)) {
    records_manager_.RemoveVisibleRecord(object);
    need_update_timing_at_frame_end_ = true;
  } else if (records_manager_.IsKnownInvisible(object)) {
    records_manager_.RemoveInvisibleRecord(object);
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
  if (!is_recording_)
    return;
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
  return !records_manager_.HasRecorded(object);
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
    records_manager_.RecordInvisibleObject(aggregator);
  } else {
    records_manager_.RecordVisibleObject(
        aggregator, aggregated_size,
        TextElementTiming::ComputeIntersectionRect(
            node, aggregated_visual_rect, property_tree_state, frame_view_),
        node_id);
  }

  if (records_manager_.HasTooManyNodes()) {
    TRACE_EVENT_INSTANT2("loading", "TextPaintTimingDetector::OverNodeLimit",
                         TRACE_EVENT_SCOPE_THREAD, "count_size_non_zero_nodes",
                         records_manager_.CountVisibleObjects(),
                         "count_size_zero_nodes",
                         records_manager_.CountInvisibleObjects());
    StopRecordEntries();
  }
}

base::WeakPtr<TextRecord> TextPaintTimingDetector::FindLargestPaintCandidate() {
  return records_manager_.FindLargestPaintCandidate();
}

void TextPaintTimingDetector::StopRecordEntries() {
  timer_.Stop();
  is_recording_ = false;
  records_manager_.CleanUp();
}

void TextPaintTimingDetector::StopRecordingLargestTextPaint() {
  timer_.Stop();
  is_recording_ltp_ = false;
  records_manager_.CleanUpLargestContentfulPaint();
}

void TextPaintTimingDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(records_manager_);
  visitor->Trace(frame_view_);
}

TextRecordsManager::TextRecordsManager() : size_ordered_set_(&LargeTextFirst) {}

void TextRecordsManager::RemoveVisibleRecord(const LayoutObject& object) {
  DCHECK(visible_node_map_.Contains(&object));
  size_ordered_set_.erase(visible_node_map_.at(&object)->AsWeakPtr());
  visible_node_map_.erase(&object);
  // We don't need to remove elements in |texts_queued_for_paint_time_| and
  // |cached_largest_paint_candidate_| as they are weak ptr.
  is_result_invalidated_ = true;
}

void TextRecordsManager::CleanUpLargestContentfulPaint() {
  size_ordered_set_.clear();
  is_result_invalidated_ = true;
}

void TextRecordsManager::RemoveInvisibleRecord(const LayoutObject& object) {
  DCHECK(invisible_node_ids_.Contains(&object));
  invisible_node_ids_.erase(&object);
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
    DCHECK_EQ(record->paint_time, base::TimeTicks());
    record->paint_time = timestamp;
  }
  if (text_element_timing_)
    text_element_timing_->OnTextNodesPainted(texts_queued_for_paint_time_);
  texts_queued_for_paint_time_.clear();
  is_result_invalidated_ = true;
}

void TextRecordsManager::RecordVisibleObject(
    const LayoutObject& object,
    const uint64_t& visual_size,
    const FloatRect& element_timing_rect,
    DOMNodeId node_id) {
  DCHECK(!HasTooManyNodes());
  DCHECK_GT(visual_size, 0u);

  std::unique_ptr<TextRecord> record =
      std::make_unique<TextRecord>(node_id, visual_size, element_timing_rect);
  if (is_recording_ltp_)
    size_ordered_set_.emplace(record->AsWeakPtr());
  QueueToMeasurePaintTime(record->AsWeakPtr());
  visible_node_map_.insert(&object, std::move(record));
  is_result_invalidated_ = true;
}

bool TextRecordsManager::HasTooManyNodes() const {
  return visible_node_map_.size() + invisible_node_ids_.size() >=
         kTextNodeNumberLimit;
}

base::WeakPtr<TextRecord> TextRecordsManager::FindLargestPaintCandidate() {
  DCHECK_EQ(visible_node_map_.size(), size_ordered_set_.size());
  if (!is_result_invalidated_ && cached_largest_paint_candidate_)
    return cached_largest_paint_candidate_;
  base::WeakPtr<TextRecord> new_largest_paint_candidate = nullptr;
  for (const auto& text_record : size_ordered_set_) {
    DCHECK(text_record);
    if (text_record->paint_time.is_null())
      continue;
    new_largest_paint_candidate = text_record;
    break;
  }
  cached_largest_paint_candidate_ = new_largest_paint_candidate;
  is_result_invalidated_ = false;
  return new_largest_paint_candidate;
}

void TextRecordsManager::CleanUp() {
  visible_node_map_.clear();
  invisible_node_ids_.clear();
  texts_queued_for_paint_time_.clear();
  CleanUpLargestContentfulPaint();
}

void TextRecordsManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(text_element_timing_);
}

}  // namespace blink
