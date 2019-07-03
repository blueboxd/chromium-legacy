// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LARGEST_CONTENTFUL_PAINT_HANDLER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LARGEST_CONTENTFUL_PAINT_HANDLER_H_

#include <map>

#include "base/trace_event/traced_value.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"

namespace page_load_metrics {

class ContentfulPaintTimingInfo {
 public:
  explicit ContentfulPaintTimingInfo(
      page_load_metrics::PageLoadMetricsObserver::LargestContentType,
      bool in_main_frame);
  explicit ContentfulPaintTimingInfo(
      const base::Optional<base::TimeDelta>&,
      const uint64_t& size,
      const page_load_metrics::PageLoadMetricsObserver::LargestContentType,
      bool in_main_frame);
  explicit ContentfulPaintTimingInfo(const ContentfulPaintTimingInfo& other);
  void Reset(const base::Optional<base::TimeDelta>&, const uint64_t& size);
  base::Optional<base::TimeDelta> Time() const { return time_; }
  bool InMainFrame() const { return in_main_frame_; }
  uint64_t Size() const { return size_; }
  page_load_metrics::PageLoadMetricsObserver::LargestContentType Type() const {
    return type_;
  }

  bool IsEmpty() const {
    // |size_| is not necessarily 0, for example, when the largest image is
    // still loading.
    return !time_;
  }

  std::unique_ptr<base::trace_event::TracedValue> DataAsTraceValue() const;

 private:
  ContentfulPaintTimingInfo() = delete;
  std::string TypeInString() const;
  base::Optional<base::TimeDelta> time_;
  uint64_t size_;
  page_load_metrics::PageLoadMetricsObserver::LargestContentType type_;
  bool in_main_frame_;
};

class ContentfulPaint {
 public:
  explicit ContentfulPaint(bool in_main_frame);
  ContentfulPaintTimingInfo& Text() { return text_; }
  ContentfulPaintTimingInfo& Image() { return image_; }
  const ContentfulPaintTimingInfo& MergeTextAndImageTiming();

 private:
  ContentfulPaintTimingInfo text_;
  ContentfulPaintTimingInfo image_;
};

class LargestContentfulPaintHandler {
 public:
  using FrameTreeNodeId =
      page_load_metrics::PageLoadMetricsObserver::FrameTreeNodeId;
  static void SetTestMode(bool enabled);
  LargestContentfulPaintHandler();
  ~LargestContentfulPaintHandler();
  void RecordTiming(const page_load_metrics::mojom::PaintTimingPtr&,
                    content::RenderFrameHost* subframe_rfh);
  inline void RecordMainFrameTreeNodeId(int main_frame_tree_node_id) {
    main_frame_tree_node_id_.emplace(main_frame_tree_node_id);
  }

  inline int MainFrameTreeNodeId() const {
    return main_frame_tree_node_id_.value();
  }

  // We merge the candidates from main frame and subframe to get the largest
  // candidate across all frames.
  const ContentfulPaintTimingInfo& MergeMainFrameAndSubframes();
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle,
      const page_load_metrics::PageLoadExtraInfo& extra_info);

 private:
  void RecordSubframeTiming(const mojom::PaintTimingPtr& timing,
                            const base::TimeDelta& navigation_start_offset);
  void RecordMainFrameTiming(const page_load_metrics::mojom::PaintTimingPtr&);
  ContentfulPaint main_frame_contentful_paint_;
  ContentfulPaint subframe_contentful_paint_;

  // Used for Telemetry to distinguish the LCP events from different
  // navigations.
  base::Optional<int> main_frame_tree_node_id_;

  // Navigation start offsets for the most recently committed document in each
  // frame.
  std::map<FrameTreeNodeId, base::TimeDelta> subframe_navigation_start_offset_;
  DISALLOW_COPY_AND_ASSIGN(LargestContentfulPaintHandler);
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LARGEST_CONTENTFUL_PAINT_HANDLER_H_
