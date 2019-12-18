// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_CROSTINI_FILE_TASKS_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_CROSTINI_FILE_TASKS_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/file_manager/file_tasks.h"

class Profile;

namespace extensions {
struct EntryInfo;
}

namespace storage {
class FileSystemURL;
}

namespace file_manager {
namespace file_tasks {

// Crostini apps all use the same action ID.
constexpr char kCrostiniAppActionID[] = "open-with";

// Finds the Crostini tasks that can handle |entries|, appends them to
// Finds the Crostini |app_ids| and |app_names| that can handle |entries|.
// VisibleForTesting.  Called by |FindCrostiniTasks|.
void FindCrostiniApps(Profile* profile,
                      const std::vector<extensions::EntryInfo>& entries,
                      std::vector<std::string>* app_ids,
                      std::vector<std::string>* app_names);

// |result_list|, and calls back to |callback| once finished.
void FindCrostiniTasks(Profile* profile,
                       const std::vector<extensions::EntryInfo>& entries,
                       std::vector<FullTaskDescriptor>* result_list,
                       base::OnceClosure completion_closure);

// Executes the specified task by Crostini.
void ExecuteCrostiniTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    FileTaskFinishedCallback done);

}  // namespace file_tasks
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_CROSTINI_FILE_TASKS_H_
