// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/site_data_recorder.h"

#include "base/time/time.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_cache.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/persistence/site_data/site_data_writer.h"

namespace performance_manager {

// The period of time after loading during which we ignore title/favicon
// change events. It's possible for some site that are loaded in background to
// use some of these features without this being an attempt to communicate
// with the user (e.g. the page is just really finishing to load).
constexpr base::TimeDelta kTitleOrFaviconChangePostLoadGracePeriod =
    base::TimeDelta::FromSeconds(20);

// The period of time during which audio usage gets ignored after a page gets
// backgrounded. It's necessary because there might be a delay between a media
// request gets initiated and the time the audio actually starts.
constexpr base::TimeDelta kFeatureUsagePostBackgroundGracePeriod =
    base::TimeDelta::FromSeconds(10);

// Provides SiteData machinery access to some internals of a PageNodeImpl.
class SiteDataAccess {
 public:
  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      PageNodeImpl* page_node) {
    return &page_node->site_data_;
  }
};

namespace {

// NodeAttachedData used to adorn every page node with a SiteDataWriter.
class SiteDataNodeData : public NodeAttachedDataImpl<SiteDataNodeData>,
                         public SiteDataRecorder::Data {
 public:
  struct Traits : public NodeAttachedDataOwnedByNodeType<PageNodeImpl> {};

  explicit SiteDataNodeData(const PageNodeImpl* page_node) {}
  ~SiteDataNodeData() override = default;

  // NodeAttachedData:
  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      PageNodeImpl* page_node) {
    return SiteDataAccess::GetUniquePtrStorage(page_node);
  }

  // Set the SiteDataCache that should be used to create the writer.
  void set_data_cache(SiteDataCache* data_cache) {
    DCHECK(data_cache);
    data_cache_ = data_cache;
  }

  // Functions called whenever one of the tracked properties changes.
  void OnMainFrameUrlChanged(const GURL& url, bool page_is_visible);
  void OnIsLoadingChanged(bool is_loading);
  void OnIsVisibleChanged(bool is_visible);
  void OnIsAudibleChanged(bool audible);
  void OnTitleUpdated();
  void OnFaviconUpdated();

  SiteDataWriter* writer() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return writer_.get();
  }

 private:
  // The features tracked by the SiteDataRecorder class.
  enum class FeatureType {
    kTitleChange,
    kFaviconChange,
    kAudioUsage,
  };

  void SetDataCacheForTesting(SiteDataCache* cache) override {
    set_data_cache(cache);
  }

  // Indicates of a feature usage event should be ignored.
  bool ShouldIgnoreFeatureUsageEvent(FeatureType feature_type);

  // Records a feature usage event if necessary.
  void MaybeNotifyBackgroundFeatureUsage(void (SiteDataWriter::*method)(),
                                         FeatureType feature_type);

  // Update |backgrounded_time_| depending on the visibility of the page.
  void UpdateBackgroundedTime();

  // The SiteDataCache used to serve writers for the PageNode owned by this
  // object.
  SiteDataCache* data_cache_ = nullptr;

  bool is_visible_ = false;
  bool is_loaded_ = false;

  // The Origin tracked by the writer.
  url::Origin last_origin_;

  // The time at which this tab has been backgrounded, null if this tab is
  // currently visible.
  base::TimeTicks backgrounded_time_;

  // The time at which this tab switched to the loaded state, null if this tab
  // is not currently loaded.
  base::TimeTicks loaded_time_;

  std::unique_ptr<SiteDataWriter> writer_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SiteDataNodeData);
};

void SiteDataNodeData::OnMainFrameUrlChanged(const GURL& url,
                                             bool page_is_visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url::Origin origin = url::Origin::Create(url);

  if (origin == last_origin_)
    return;

  // If the origin has changed then the writer should be invalidated.
  writer_.reset();

  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  last_origin_ = origin;
  writer_ = data_cache_->GetWriterForOrigin(
      origin, page_is_visible ? TabVisibility::kForeground
                              : TabVisibility::kBackground);

  is_visible_ = page_is_visible;
  UpdateBackgroundedTime();
}

void SiteDataNodeData::OnIsLoadingChanged(bool is_loading) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!writer_)
    return;
  if (is_loading) {
    is_loaded_ = false;
    writer_->NotifySiteUnloaded();
    loaded_time_ = base::TimeTicks();
  } else {
    is_loaded_ = true;
    writer_->NotifySiteLoaded();
    loaded_time_ = base::TimeTicks::Now();
  }
}

void SiteDataNodeData::OnIsVisibleChanged(bool is_visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!writer_)
    return;
  is_visible_ = is_visible;
  UpdateBackgroundedTime();
  writer_->NotifySiteVisibilityChanged(is_visible ? TabVisibility::kForeground
                                                  : TabVisibility::kBackground);
}

void SiteDataNodeData::OnIsAudibleChanged(bool audible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!audible)
    return;

  MaybeNotifyBackgroundFeatureUsage(
      &SiteDataWriter::NotifyUsesAudioInBackground, FeatureType::kAudioUsage);
}

void SiteDataNodeData::OnTitleUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeNotifyBackgroundFeatureUsage(
      &SiteDataWriter::NotifyUpdatesTitleInBackground,
      FeatureType::kTitleChange);
}

void SiteDataNodeData::OnFaviconUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeNotifyBackgroundFeatureUsage(
      &SiteDataWriter::NotifyUpdatesFaviconInBackground,
      FeatureType::kFaviconChange);
}

bool SiteDataNodeData::ShouldIgnoreFeatureUsageEvent(FeatureType feature_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The feature usage should be ignored if there's no writer for this page.
  if (!writer_)
    return true;

  // Ignore all features happening before the website gets fully loaded.
  if (!is_loaded_)
    return true;

  // Ignore events if the tab is not in background.
  if (is_visible_)
    return true;

  if (feature_type == FeatureType::kTitleChange ||
      feature_type == FeatureType::kFaviconChange) {
    DCHECK(!loaded_time_.is_null());
    if (base::TimeTicks::Now() - loaded_time_ <
        kTitleOrFaviconChangePostLoadGracePeriod) {
      return true;
    }
  }

  // Ignore events happening shortly after the tab being backgrounded, they're
  // usually false positives.
  DCHECK(!backgrounded_time_.is_null());
  if ((base::TimeTicks::Now() - backgrounded_time_ <
       kFeatureUsagePostBackgroundGracePeriod)) {
    return true;
  }

  return false;
}

void SiteDataNodeData::MaybeNotifyBackgroundFeatureUsage(
    void (SiteDataWriter::*method)(),
    FeatureType feature_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldIgnoreFeatureUsageEvent(feature_type))
    return;

  (writer_.get()->*method)();
}

void SiteDataNodeData::UpdateBackgroundedTime() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_visible_) {
    backgrounded_time_ = base::TimeTicks();
  } else {
    backgrounded_time_ = base::TimeTicks::Now();
  }
}

SiteDataNodeData* GetSiteDataNodeDataFromPageNode(const PageNode* page_node) {
  auto* page_node_impl = PageNodeImpl::FromNode(page_node);
  DCHECK(page_node_impl);
  auto* data = SiteDataNodeData::Get(page_node_impl);
  DCHECK(data);
  return data;
}

}  // namespace

// static
SiteDataRecorder::Data* SiteDataRecorder::Data::GetForTesting(
    const PageNode* page_node) {
  return GetSiteDataNodeDataFromPageNode(page_node);
}

SiteDataRecorder::SiteDataRecorder() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SiteDataRecorder::~SiteDataRecorder() = default;

void SiteDataRecorder::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RegisterObservers(graph);
}

void SiteDataRecorder::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UnregisterObservers(graph);
}

void SiteDataRecorder::OnPageNodeAdded(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetPageNodeDataCache(page_node);
}

void SiteDataRecorder::OnMainFrameUrlChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  data->OnMainFrameUrlChanged(page_node->GetMainFrameUrl(),
                              page_node->IsVisible());
}

void SiteDataRecorder::OnIsLoadingChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  data->OnIsLoadingChanged(page_node->IsLoading());
}

void SiteDataRecorder::OnIsVisibleChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  data->OnIsVisibleChanged(page_node->IsVisible());
}

void SiteDataRecorder::OnIsAudibleChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  data->OnIsAudibleChanged(page_node->IsAudible());
}

void SiteDataRecorder::OnTitleUpdated(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  data->OnTitleUpdated();
}

void SiteDataRecorder::OnFaviconUpdated(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  data->OnFaviconUpdated();
}

void SiteDataRecorder::RegisterObservers(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddPageNodeObserver(this);
}

void SiteDataRecorder::UnregisterObservers(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemovePageNodeObserver(this);
}

void SiteDataRecorder::SetPageNodeDataCache(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* page_node_impl = PageNodeImpl::FromNode(page_node);
  DCHECK(page_node_impl);
  DCHECK(!SiteDataNodeData::Get(page_node_impl));
  auto* data = SiteDataNodeData::GetOrCreate(page_node_impl);
  data->set_data_cache(
      SiteDataCacheFactory::GetInstance()->GetDataCacheForBrowserContext(
          page_node->GetBrowserContextID()));
}

SiteDataNodeData::Data::Data() = default;
SiteDataNodeData::Data::~Data() = default;

}  // namespace performance_manager
