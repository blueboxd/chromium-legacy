// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/metrics_collector.h"

#include <set>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl_operations.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_clock.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

namespace performance_manager {

// Delay the metrics report from GRC to UMA/UKM for 5 minutes from when the main
// frame navigation is committed.
const base::TimeDelta kMetricsReportDelayTimeout =
    base::TimeDelta::FromMinutes(5);

const char kTabFromBackgroundedToFirstFaviconUpdatedUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstFaviconUpdated";
const char kTabFromBackgroundedToFirstTitleUpdatedUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstTitleUpdated";
const char kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA[] =
    "TabManager.Heuristics."
    "FromBackgroundedToFirstNonPersistentNotificationCreated";

const int kDefaultFrequencyUkmEQTReported = 5u;

// Gets the number of tabs that are co-resident in all of the render processes
// associated with a PageNode.
size_t GetNumCoresidentTabs(const PageNodeImpl* page_node) {
  std::set<NodeBase*> coresident_tabs;
  auto process_nodes =
      GraphImplOperations::GetAssociatedProcessNodes(page_node);
  for (auto* process_node : process_nodes) {
    auto page_nodes = GraphImplOperations::GetAssociatedPageNodes(process_node);
    for (auto* associated_page_node : page_nodes) {
      coresident_tabs.insert(associated_page_node);
    }
  }
  // A tab cannot be co-resident with itself.
  return coresident_tabs.size() - 1;
}

MetricsCollector::MetricsCollector() {
  UpdateWithFieldTrialParams();
}

MetricsCollector::~MetricsCollector() = default;

bool MetricsCollector::ShouldObserve(const NodeBase* node) {
  return node->type() == FrameNodeImpl::Type() ||
         node->type() == PageNodeImpl::Type() ||
         node->type() == ProcessNodeImpl::Type();
}

void MetricsCollector::OnNodeAdded(NodeBase* node) {
  if (node->type() == PageNodeImpl::Type()) {
    PageNodeImpl* page_node = PageNodeImpl::FromNodeBase(node);
    metrics_report_record_map_.emplace(page_node, MetricsReportRecord());
  }
}

void MetricsCollector::OnBeforeNodeRemoved(NodeBase* node) {
  if (node->type() == PageNodeImpl::Type()) {
    PageNodeImpl* page_node = PageNodeImpl::FromNodeBase(node);
    metrics_report_record_map_.erase(page_node);
    ukm_collection_state_map_.erase(page_node);
  }
}

void MetricsCollector::OnNonPersistentNotificationCreated(
    FrameNodeImpl* frame_node) {
  // Only record metrics while a page is backgrounded.
  auto* page_node = frame_node->page_node();
  if (page_node->is_visible() || !ShouldReportMetrics(page_node))
    return;

  MetricsReportRecord& record =
      metrics_report_record_map_.find(page_node)->second;
  record.first_non_persistent_notification_created.OnSignalReceived(
      frame_node->IsMainFrame(), page_node->TimeSinceLastVisibilityChange(),
      graph()->ukm_recorder());
}

void MetricsCollector::OnIsVisibleChanged(PageNodeImpl* page_node) {
  // The page becomes visible again, clear all records in order to
  // report metrics when page becomes invisible next time.
  if (page_node->is_visible())
    ResetMetricsReportRecord(page_node);
}

void MetricsCollector::OnUkmSourceIdChanged(PageNodeImpl* page_node) {
  ukm::SourceId ukm_source_id = page_node->ukm_source_id();
  UpdateUkmSourceIdForPage(page_node, ukm_source_id);
  MetricsReportRecord& record =
      metrics_report_record_map_.find(page_node)->second;
  record.UpdateUkmSourceID(ukm_source_id);
}

void MetricsCollector::OnFaviconUpdated(PageNodeImpl* page_node) {
  // Only record metrics while it is backgrounded.
  if (page_node->is_visible() || !ShouldReportMetrics(page_node))
    return;
  MetricsReportRecord& record =
      metrics_report_record_map_.find(page_node)->second;
  record.first_favicon_updated.OnSignalReceived(
      true, page_node->TimeSinceLastVisibilityChange(),
      graph()->ukm_recorder());
}

void MetricsCollector::OnTitleUpdated(PageNodeImpl* page_node) {
  // Only record metrics while it is backgrounded.
  if (page_node->is_visible() || !ShouldReportMetrics(page_node))
    return;
  MetricsReportRecord& record =
      metrics_report_record_map_.find(page_node)->second;
  record.first_title_updated.OnSignalReceived(
      true, page_node->TimeSinceLastVisibilityChange(),
      graph()->ukm_recorder());
}

void MetricsCollector::OnExpectedTaskQueueingDurationSample(
    ProcessNodeImpl* process_node) {
  // Report this measurement to all pages that are hosting a main frame in
  // the process that was sampled.
  const base::TimeDelta& sample =
      process_node->expected_task_queueing_duration();
  for (auto* frame_node : process_node->frame_nodes()) {
    if (!frame_node->IsMainFrame())
      continue;
    auto* page_node = frame_node->page_node();
    if (!IsCollectingExpectedQueueingTimeForUkm(page_node))
      continue;
    RecordExpectedQueueingTimeForUkm(page_node, sample);
  }
}

bool MetricsCollector::ShouldReportMetrics(const PageNodeImpl* page_node) {
  return page_node->TimeSinceLastNavigation() > kMetricsReportDelayTimeout;
}

bool MetricsCollector::IsCollectingExpectedQueueingTimeForUkm(
    PageNodeImpl* page_node) {
  UkmCollectionState& state = ukm_collection_state_map_[page_node];
  return state.ukm_source_id != ukm::kInvalidSourceId &&
         ++state.num_unreported_eqt_measurements >= frequency_ukm_eqt_reported_;
}

void MetricsCollector::RecordExpectedQueueingTimeForUkm(
    PageNodeImpl* page_node,
    const base::TimeDelta& expected_queueing_time) {
  UkmCollectionState& state = ukm_collection_state_map_[page_node];
  state.num_unreported_eqt_measurements = 0u;
  ukm::builders::ResponsivenessMeasurement(state.ukm_source_id)
      .SetExpectedTaskQueueingDuration(expected_queueing_time.InMilliseconds())
      .Record(graph()->ukm_recorder());
}

void MetricsCollector::UpdateUkmSourceIdForPage(PageNodeImpl* page_node,
                                                ukm::SourceId ukm_source_id) {
  UkmCollectionState& state = ukm_collection_state_map_[page_node];

  state.ukm_source_id = ukm_source_id;
  // Updating the |ukm_source_id| restarts usage collection.
  state.num_unreported_eqt_measurements = 0u;
}

void MetricsCollector::UpdateWithFieldTrialParams() {
  frequency_ukm_eqt_reported_ = base::GetFieldTrialParamByFeatureAsInt(
      ukm::kUkmFeature, "FrequencyUKMExpectedQueueingTime",
      kDefaultFrequencyUkmEQTReported);
}

void MetricsCollector::ResetMetricsReportRecord(PageNodeImpl* page_node) {
  DCHECK(metrics_report_record_map_.find(page_node) !=
         metrics_report_record_map_.end());
  metrics_report_record_map_.find(page_node)->second.Reset();
}

MetricsCollector::MetricsReportRecord::MetricsReportRecord() = default;

MetricsCollector::MetricsReportRecord::MetricsReportRecord(
    const MetricsReportRecord& other) = default;

void MetricsCollector::MetricsReportRecord::UpdateUkmSourceID(
    ukm::SourceId ukm_source_id) {
  first_favicon_updated.SetUkmSourceID(ukm_source_id);
  first_non_persistent_notification_created.SetUkmSourceID(ukm_source_id);
  first_title_updated.SetUkmSourceID(ukm_source_id);
}

void MetricsCollector::MetricsReportRecord::Reset() {
  first_favicon_updated.Reset();
  first_non_persistent_notification_created.Reset();
  first_title_updated.Reset();
}

}  // namespace performance_manager
