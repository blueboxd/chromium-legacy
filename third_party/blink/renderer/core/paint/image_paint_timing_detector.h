// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_PAINT_TIMING_DETECTOR_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class LayoutObject;
class LocalFrameView;
class PropertyTreeState;
class TracedValue;
class Image;

// TODO(crbug/960502): we should limit the access of these properties.
class ImageRecord : public base::SupportsWeakPtr<ImageRecord> {
 public:
  ImageRecord(DOMNodeId new_node_id,
              const ImageResourceContent* new_cached_image,
              uint64_t new_first_size)
      : node_id(new_node_id),
        cached_image(new_cached_image),
        first_size(new_first_size) {
    static unsigned next_insertion_index_ = 1;
    insertion_index = next_insertion_index_++;
  }

  ImageRecord() {}

  DOMNodeId node_id = kInvalidDOMNodeId;
  WeakPersistent<const ImageResourceContent> cached_image;
  // Mind that |first_size| has to be assigned before pusing to
  // |size_ordered_set_| since it's the sorting key.
  uint64_t first_size = 0;
  unsigned frame_index = 0;
  unsigned insertion_index;
  // The time of the first paint after fully loaded. 0 means not painted yet.
  base::TimeTicks paint_time = base::TimeTicks();
  bool loaded = false;
};

typedef std::pair<DOMNodeId, const ImageResourceContent*> BackgroundImageId;

// |ImageRecordsManager| is the manager of all of the images that Largest Image
// Paint cares about. Note that an image does not necessarily correspond to a
// node; it can also be one of the background images attached to a node.
// |ImageRecordsManager| encapsulates the logic of |ImageRecord| handling,
// providing interface for the external world to handle it in the language of
// Node, LayoutObject, etc.
class CORE_EXPORT ImageRecordsManager {
  friend class ImagePaintTimingDetectorTest;
  using NodesQueueComparator = bool (*)(const base::WeakPtr<ImageRecord>&,
                                        const base::WeakPtr<ImageRecord>&);
  using ImageRecordSet =
      std::set<base::WeakPtr<ImageRecord>, NodesQueueComparator>;

 public:
  ImageRecordsManager();
  ImageRecord* FindLargestPaintCandidate() const;

  bool AreAllVisibleNodesDetached() const;

  inline void RemoveInvisibleRecordIfNeeded(
      const DOMNodeId& invisible_node_id) {
    invisible_node_ids_.erase(invisible_node_id);
  }
  inline void RemoveVisibleRecord(
      const BackgroundImageId& background_image_id) {
    base::WeakPtr<ImageRecord> record =
        visible_background_image_map_.find(background_image_id)
            ->value->AsWeakPtr();
    size_ordered_set_.erase(record);
    visible_background_image_map_.erase(background_image_id);
    // Leave out |images_queued_for_paint_time_| intentionally because the null
    // record will be removed in |AssignPaintTimeToRegisteredQueuedNodes|.
  }

  inline void RecordInvisibleNode(const DOMNodeId& node_id) {
    invisible_node_ids_.insert(node_id);
  }
  void RecordVisibleNode(const BackgroundImageId& background_image_id,
                         const uint64_t& visual_size);
  size_t CountInvisibleNodes() const { return invisible_node_ids_.size(); }
  bool IsRecordedVisibleNode(
      const BackgroundImageId& background_image_id) const {
    return visible_background_image_map_.Contains(background_image_id);
  }
  bool IsRecordedInvisibleNode(const DOMNodeId& node_id) const {
    return invisible_node_ids_.Contains(node_id);
  }

  inline bool WasVisibleNodeLoaded(
      const BackgroundImageId& background_image_id) const {
    DCHECK(visible_background_image_map_.Contains(background_image_id));
    return visible_background_image_map_.at(background_image_id)->loaded;
  }
  void OnImageLoaded(const BackgroundImageId&, unsigned current_frame_index);
  void OnImageLoadedInternal(base::WeakPtr<ImageRecord>&,
                             unsigned current_frame_index);

  inline bool NeedMeausuringPaintTime() const {
    return !images_queued_for_paint_time_.IsEmpty();
  }

  // Compare the last frame index in queue with the last frame index that has
  // registered for assigning paint time.
  inline bool HasUnregisteredRecordsInQueued(
      unsigned last_registered_frame_index) {
    while (!images_queued_for_paint_time_.IsEmpty() &&
           !images_queued_for_paint_time_.back()) {
      images_queued_for_paint_time_.pop_back();
    }
    if (images_queued_for_paint_time_.IsEmpty())
      return false;
    DCHECK(last_registered_frame_index <= LastQueuedFrameIndex());
    return last_registered_frame_index < LastQueuedFrameIndex();
  }
  void AssignPaintTimeToRegisteredQueuedNodes(const base::TimeTicks&,
                                              unsigned last_queued_frame_index);
  inline unsigned LastQueuedFrameIndex() const {
    DCHECK(images_queued_for_paint_time_.back());
    return images_queued_for_paint_time_.back()->frame_index;
  }

 private:
  // Find the image record of an visible image.
  inline base::WeakPtr<ImageRecord> FindVisibleRecord(
      const BackgroundImageId& background_image_id) const {
    DCHECK(visible_background_image_map_.Contains(background_image_id));
    return visible_background_image_map_.find(background_image_id)
        ->value->AsWeakPtr();
  }
  std::unique_ptr<ImageRecord> CreateImageRecord(
      const DOMNodeId&,
      const ImageResourceContent* cached_image,
      const uint64_t& visual_size);
  inline void QueueToMeasurePaintTime(base::WeakPtr<ImageRecord>& record,
                                      unsigned current_frame_index) {
    images_queued_for_paint_time_.push_back(record);
    record->frame_index = current_frame_index;
  }
  inline void SetLoaded(base::WeakPtr<ImageRecord>& record) {
    record->loaded = true;
  }

  HashMap<BackgroundImageId, std::unique_ptr<ImageRecord>>
      visible_background_image_map_;
  HashSet<DOMNodeId> invisible_node_ids_;

  // This stores the image records, which are ordered by size.
  ImageRecordSet size_ordered_set_;
  // |ImageRecord|s waiting for paint time are stored in this queue
  // until they get a swap time.
  Deque<base::WeakPtr<ImageRecord>> images_queued_for_paint_time_;

  DISALLOW_COPY_AND_ASSIGN(ImageRecordsManager);
};

// ImagePaintTimingDetector contains Largest Image Paint.
//
// Largest Image Paint timing measures when the largest image element within
// viewport finishes painting. Specifically, it:
// 1. Tracks all images' first invalidation, recording their visual size, if
// this image is within viewport.
// 2. When an image finishes loading, record its paint time.
// 3. At the end of each frame, if new images are added and loaded, the
// algorithm will start an analysis.
//
// In the analysis:
// 3.1 Largest Image Paint finds the largest image by the first visual size. If
// it has finished loading, reports a candidate result as its first paint time
// since loaded.
//
// For all these candidate results, Telemetry picks the lastly reported
// Largest Image Paint candidate as its final result.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit#heading=h.1k2rnrs6mdmt
class CORE_EXPORT ImagePaintTimingDetector final
    : public GarbageCollectedFinalized<ImagePaintTimingDetector> {
  friend class ImagePaintTimingDetectorTest;

 public:
  ImagePaintTimingDetector(LocalFrameView*);
  void RecordImage(const LayoutObject&,
                   const IntSize& intrinsic_size,
                   const ImageResourceContent&,
                   const PropertyTreeState& current_paint_chunk_properties);
  void RecordBackgroundImage(
      const LayoutObject&,
      const IntSize& intrinsic_size,
      const ImageResourceContent& cached_image,
      const PropertyTreeState& current_paint_chunk_properties);
  void OnPaintFinished();
  void LayoutObjectWillBeDestroyed(const LayoutObject&);
  void NotifyBackgroundImageRemoved(DOMNodeId, const ImageResourceContent*);
  // After the method being called, the detector stops to record new entries and
  // node removal. But it still observe the loading status. In other words, if
  // an image is recorded before stopping recording, and finish loading after
  // stopping recording, the detector can still observe the loading being
  // finished.
  inline void StopRecordEntries() { is_recording_ = false; }
  inline bool IsRecording() const { return is_recording_; }
  inline bool FinishedReportingImages() const {
    return !is_recording_ && num_pending_swap_callbacks_ == 0;
  }

  void Trace(blink::Visitor*);

 private:
  friend class LargestContentfulPaintCalculatorTest;

  ImageRecord* FindLargestPaintCandidate() const;

  void PopulateTraceValue(TracedValue&, const ImageRecord& first_image_paint);
  // This is provided for unit test to force invoking swap promise callback.
  void ReportSwapTime(unsigned last_queued_frame_index,
                      WebWidgetClient::SwapResult,
                      base::TimeTicks);
  void RegisterNotifySwapTime();
  void ReportCandidateToTrace(ImageRecord&);
  void ReportNoCandidateToTrace();
  void Deactivate();

  void UpdateCandidate();

  base::RepeatingCallback<void(WebWidgetClient::ReportTimeCallback)>
      notify_swap_time_override_for_testing_;

  // Used to find the last candidate.
  unsigned count_candidates_ = 0;

  // Used to decide which frame a record belongs to, monotonically increasing.
  unsigned frame_index_ = 1;
  unsigned last_registered_frame_index_ = 0;

  // Used to control if we record new image entries and image removal, but has
  // no effect on recording the loading status.
  bool is_recording_ = true;

  // Used to determine how many swap callbacks are pending. In combination with
  // |is_recording|, helps determine whether this detector can be destroyed.
  int num_pending_swap_callbacks_ = 0;

  // This need to be set whenever changes that can affect the output of
  // |FindLargestPaintCandidate| occur during the paint tree walk.
  bool need_update_timing_at_frame_end_ = false;

  ImageRecordsManager records_manager_;
  Member<LocalFrameView> frame_view_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_PAINT_TIMING_DETECTOR_H_
