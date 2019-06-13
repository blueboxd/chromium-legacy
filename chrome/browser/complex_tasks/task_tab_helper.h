// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPLEX_TASKS_TASK_TAB_HELPER_H_
#define CHROME_BROWSER_COMPLEX_TASKS_TASK_TAB_HELPER_H_

#include <map>

#include "base/macros.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/sessions/content/content_record_task_id.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace sessions {
class ContextRecordTaskId;
}

namespace tasks {

// This is a tab helper that collects navigation state information of a
// complex task.
class TaskTabHelper : public content::WebContentsObserver,
                      public content::WebContentsUserData<TaskTabHelper> {
 public:
  ~TaskTabHelper() override;

  // WebContentsObserver
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void NavigationListPruned(
      const content::PrunedDetails& pruned_details) override;
  static sessions::ContextRecordTaskId* GetContextRecordTaskId(
      content::WebContents* web_contents);
  const sessions::ContextRecordTaskId* get_context_record_task_id(
      int nav_id) const {
    if (!base::Contains(local_context_record_task_id_map_, nav_id))
      return nullptr;
    return &local_context_record_task_id_map_.find(nav_id)->second;
  }

 protected:
  explicit TaskTabHelper(content::WebContents* web_contents);

  enum class HubType { DEFAULT_SEARCH_ENGINE, FORM_SUBMIT, OTHER };

  virtual HubType GetSpokeEntryHubType() const;

  // For testing
  int GetSpokesForTesting(int id) {
    return entry_index_to_spoke_count_map_[id];
  }

 private:
  friend class content::WebContentsUserData<TaskTabHelper>;
  void UpdateAndRecordTaskIds(
      const content::LoadCommittedDetails& load_details);

  void RecordHubAndSpokeNavigationUsage(int sample);

#if defined(OS_ANDROID)
  int64_t GetParentTaskId();
  int64_t GetParentRootTaskId();
#endif  // defined(OS_ANDROID)

  int last_pruned_navigation_entry_index_;
  std::map<int, int> entry_index_to_spoke_count_map_;
  std::unordered_map<int, sessions::ContextRecordTaskId>
      local_context_record_task_id_map_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(TaskTabHelper);
};

}  // namespace tasks

#endif  // CHROME_BROWSER_COMPLEX_TASKS_TASK_TAB_HELPER_H_
