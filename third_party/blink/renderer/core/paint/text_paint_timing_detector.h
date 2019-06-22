// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_

#include <memory>
#include <queue>
#include <set>

#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/paint/text_element_timing.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class LayoutBoxModelObject;
class LocalFrameView;
class PropertyTreeState;
class TextElementTiming;
class TracedValue;

class TextRecord : public base::SupportsWeakPtr<TextRecord> {
 public:
  TextRecord(DOMNodeId new_node_id,
             uint64_t new_first_size,
             const FloatRect& element_timing_rect)
      : node_id(new_node_id),
        first_size(new_first_size),
        element_timing_rect_(element_timing_rect) {}

  DOMNodeId node_id = kInvalidDOMNodeId;
  uint64_t first_size = 0;
  FloatRect element_timing_rect_;
  // The time of the first paint after fully loaded.
  base::TimeTicks paint_time = base::TimeTicks();
  DISALLOW_COPY_AND_ASSIGN(TextRecord);
};

class TextRecordsManager {
  DISALLOW_NEW();
  using TextRecordSetComparator = bool (*)(const base::WeakPtr<TextRecord>&,
                                           const base::WeakPtr<TextRecord>&);
  using TextRecordSet =
      std::set<base::WeakPtr<TextRecord>, TextRecordSetComparator>;
  friend class TextPaintTimingDetectorTest;

 public:
  TextRecordsManager();
  base::WeakPtr<TextRecord> FindLargestPaintCandidate();

  void RemoveVisibleRecord(const LayoutObject&);
  void RemoveInvisibleRecord(const LayoutObject&);
  inline void RecordInvisibleObject(const LayoutObject& object) {
    DCHECK(!HasTooManyNodes());
    invisible_node_ids_.insert(&object);
  }
  void RecordVisibleObject(const LayoutObject&,
                           const uint64_t& visual_size,
                           const FloatRect& element_timing_rect,
                           DOMNodeId);
  bool NeedMeausuringPaintTime() const {
    return !texts_queued_for_paint_time_.IsEmpty();
  }
  void AssignPaintTimeToQueuedNodes(const base::TimeTicks&);

  bool HasTooManyNodes() const;
  inline bool HasRecorded(const LayoutObject& object) const {
    return visible_node_map_.Contains(&object) ||
           invisible_node_ids_.Contains(&object);
  }

  size_t CountVisibleObjects() const { return visible_node_map_.size(); }
  size_t CountInvisibleObjects() const { return invisible_node_ids_.size(); }

  inline bool IsKnownVisible(const LayoutObject& object) const {
    return visible_node_map_.Contains(&object);
  }

  inline bool IsKnownInvisible(const LayoutObject& object) const {
    return invisible_node_ids_.Contains(&object);
  }

  void CleanUp();

  void CleanUpLargestContentfulPaint();
  void StopRecordingLargestTextPaint();
  bool IsRecordingLargestTextPaint() const { return is_recording_ltp_; }

  bool HasTextElementTiming() const { return !!text_element_timing_; }
  void SetTextElementTiming(TextElementTiming* text_element_timing) {
    text_element_timing_ = text_element_timing;
  }

  void Trace(blink::Visitor*);

 private:
  inline void QueueToMeasurePaintTime(base::WeakPtr<TextRecord> record) {
    texts_queued_for_paint_time_.push_back(std::move(record));
  }
  // This is used to cache the largest text paint result for better efficiency.
  // The result will be invalidated whenever any change is done to the variables
  // used in |FindLargestPaintCandidate|.
  bool is_result_invalidated_ = false;
  // This is used to know whether |size_ordered_set_| should be populated or
  // not, since this is used by Largest Text Paint but not by Text Element
  // Timing.
  bool is_recording_ltp_ =
      RuntimeEnabledFeatures::FirstContentfulPaintPlusPlusEnabled();
  // Once LayoutObject* is destroyed, |visible_node_map_| and
  // |invisible_node_ids_| must immediately clear the corresponding record from
  // themselves.
  HashMap<const LayoutObject*, std::unique_ptr<TextRecord>> visible_node_map_;
  HashSet<const LayoutObject*> invisible_node_ids_;

  TextRecordSet size_ordered_set_;
  Deque<base::WeakPtr<TextRecord>> texts_queued_for_paint_time_;
  base::WeakPtr<TextRecord> cached_largest_paint_candidate_;
  Member<TextElementTiming> text_element_timing_;

  DISALLOW_COPY_AND_ASSIGN(TextRecordsManager);
};

// TextPaintTimingDetector contains Largest Text Paint and support for Text
// Element Timing.
//
// Largest Text Paint timing measures when the largest text element gets painted
// within viewport. Specifically, it:
// 1. Tracks all texts' first paints, recording their visual size, paint time.
// 2. Every 1 second after the first text pre-paint, the algorithm starts an
// analysis. In the analysis:
// 2.1 Largest Text Paint finds the text with the  largest first visual size,
// reports its first paint time as a candidate result.
//
// For all these candidate results, Telemetry picks the lastly reported
// Largest Text Paint candidate as the final result.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit#heading=h.lvno2v283uls
class CORE_EXPORT TextPaintTimingDetector final
    : public GarbageCollectedFinalized<TextPaintTimingDetector> {
  using ReportTimeCallback =
      WTF::CrossThreadOnceFunction<void(WebWidgetClient::SwapResult,
                                        base::TimeTicks)>;
  friend class TextPaintTimingDetectorTest;

 public:
  TextPaintTimingDetector(LocalFrameView* frame_view);
  bool ShouldWalkObject(const LayoutBoxModelObject&) const;
  void RecordAggregatedText(const LayoutBoxModelObject& aggregator,
                            const IntRect& aggregated_visual_rect,
                            const PropertyTreeState&);
  void OnPaintFinished();
  void LayoutObjectWillBeDestroyed(const LayoutObject&);
  base::WeakPtr<TextRecord> FindLargestPaintCandidate();
  void StopRecordEntries();
  void StopRecordingLargestTextPaint();
  bool IsRecording() const { return is_recording_; }
  bool FinishedReportingText() const {
    return !is_recording_ && !need_update_timing_at_frame_end_;
  }
  void Trace(blink::Visitor*);

 private:
  friend class LargestContentfulPaintCalculatorTest;

  void PopulateTraceValue(TracedValue&, const TextRecord& first_text_paint);
  void TimerFired(TimerBase*);
  void UpdateCandidate();

  void ReportSwapTime(WebWidgetClient::SwapResult result,
                      base::TimeTicks timestamp);
  void RegisterNotifySwapTime(ReportTimeCallback callback);
  void ReportCandidateToTrace(const TextRecord&);
  void ReportNoCandidateToTrace();

  TextRecordsManager records_manager_;

  // Make sure that at most one swap promise is ongoing.
  bool awaiting_swap_promise_ = false;
  unsigned count_candidates_ = 0;
  bool is_recording_ = true;
  bool is_recording_ltp_ =
      RuntimeEnabledFeatures::FirstContentfulPaintPlusPlusEnabled();

  bool has_records_changed_ = true;
  bool need_update_timing_at_frame_end_ = false;

  TaskRunnerTimer<TextPaintTimingDetector> timer_;
  Member<LocalFrameView> frame_view_;

  DISALLOW_COPY_AND_ASSIGN(TextPaintTimingDetector);
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_
