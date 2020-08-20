// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEDERATED_LEARNING_FLOC_BLOCKLIST_SERVICE_H_
#define COMPONENTS_FEDERATED_LEARNING_FLOC_BLOCKLIST_SERVICE_H_

#include <stdint.h>

#include <unordered_set>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace federated_learning {

// Responsible for loading the blocklist of flocs that are downloaded through
// the component updater.
//
// File reading and parsing is posted to |background_task_runner_|.
class FlocBlocklistService
    : public base::SupportsWeakPtr<FlocBlocklistService> {
 public:
  class Observer {
   public:
    virtual void OnBlocklistLoaded() = 0;
  };

  using LoadedBlocklist = base::Optional<std::unordered_set<uint64_t>>;

  FlocBlocklistService();
  virtual ~FlocBlocklistService();

  FlocBlocklistService(const FlocBlocklistService&) = delete;
  FlocBlocklistService& operator=(const FlocBlocklistService&) = delete;

  // Adds/Removes an Observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Virtual for testing.
  virtual void OnBlocklistFileReady(const base::FilePath& file_path);

  void SetBackgroundTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  bool BlocklistLoaded() const;
  bool ShouldBlockFloc(uint64_t floc_id) const;

 protected:
  // Virtual for testing.
  virtual void OnBlocklistLoadResult(LoadedBlocklist blocklist);

 private:
  friend class MockFlocBlocklistService;
  friend class FlocIdProviderUnitTest;
  friend class FlocIdProviderWithCustomizedServicesBrowserTest;

  // Runner for tasks that do not influence user experience.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::ObserverList<Observer>::Unchecked observers_;

  LoadedBlocklist loaded_blocklist_;
};

}  // namespace federated_learning

#endif  // COMPONENTS_FEDERATED_LEARNING_FLOC_BLOCKLIST_SERVICE_H_
