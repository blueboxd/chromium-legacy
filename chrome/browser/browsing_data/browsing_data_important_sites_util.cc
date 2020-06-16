// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"

#include "base/scoped_observer.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "content/public/browser/browsing_data_filter_builder.h"

namespace {

// This object receives |task_count| calls from BrowsingDataRemover, calls
// |callback| when all tasks are finished and destroys itself.
class BrowsingDataTaskObserver : public content::BrowsingDataRemover::Observer {
 public:
  BrowsingDataTaskObserver(content::BrowsingDataRemover* remover,
                           base::OnceClosure callback,
                           int task_count);
  ~BrowsingDataTaskObserver() override;

  void OnBrowsingDataRemoverDone() override;

 private:
  base::OnceClosure callback_;
  ScopedObserver<content::BrowsingDataRemover,
                 content::BrowsingDataRemover::Observer>
      remover_observer_;
  int task_count_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataTaskObserver);
};

BrowsingDataTaskObserver::BrowsingDataTaskObserver(
    content::BrowsingDataRemover* remover,
    base::OnceClosure callback,
    int task_count)
    : callback_(std::move(callback)),
      remover_observer_(this),
      task_count_(task_count) {
  remover_observer_.Add(remover);
}

BrowsingDataTaskObserver::~BrowsingDataTaskObserver() {}

void BrowsingDataTaskObserver::OnBrowsingDataRemoverDone() {
  DCHECK(task_count_);
  if (--task_count_)
    return;
  remover_observer_.RemoveAll();
  std::move(callback_).Run();
  delete this;
}

}  // namespace

namespace browsing_data_important_sites_util {

void Remove(uint64_t remove_mask,
            uint64_t origin_mask,
            browsing_data::TimePeriod time_period,
            std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
            content::BrowsingDataRemover* remover,
            base::OnceClosure callback) {
  auto* observer =
      new BrowsingDataTaskObserver(remover, std::move(callback), 2);

  uint64_t filterable_mask = 0;
  uint64_t nonfilterable_mask = remove_mask;

  if (!filter_builder->IsEmptyBlacklist()) {
    filterable_mask =
        remove_mask &
        ChromeBrowsingDataRemoverDelegate::IMPORTANT_SITES_DATA_TYPES;
    nonfilterable_mask =
        remove_mask &
        ~ChromeBrowsingDataRemoverDelegate::IMPORTANT_SITES_DATA_TYPES;
  }
  browsing_data::RecordDeletionForPeriod(time_period);

  if (nonfilterable_mask) {
    remover->RemoveAndReply(
        browsing_data::CalculateBeginDeleteTime(time_period),
        browsing_data::CalculateEndDeleteTime(time_period), nonfilterable_mask,
        origin_mask, observer);
  } else {
    observer->OnBrowsingDataRemoverDone();
  }

  // Cookie deletion could be deferred until all other data types are deleted.
  // As cookie deletion may be filtered, this needs to happen last.
  if (filterable_mask) {
    remover->RemoveWithFilterAndReply(
        browsing_data::CalculateBeginDeleteTime(time_period),
        browsing_data::CalculateEndDeleteTime(time_period), filterable_mask,
        origin_mask, std::move(filter_builder), observer);
  } else {
    observer->OnBrowsingDataRemoverDone();
  }
}

}  // namespace browsing_data_important_sites_util
