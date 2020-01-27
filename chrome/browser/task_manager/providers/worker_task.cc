// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/worker_task.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

int GetTaskTitlePrefixMessageId(Task::Type task_type) {
  switch (task_type) {
    case Task::Type::SERVICE_WORKER:
      return IDS_TASK_MANAGER_SERVICE_WORKER_PREFIX;
    default:
      NOTREACHED();
      return 0;
  }
}

WorkerTask::WorkerTask(base::ProcessHandle handle,
                       const GURL& script_url,
                       Task::Type task_type,
                       int render_process_id)
    : Task(l10n_util::GetStringFUTF16(GetTaskTitlePrefixMessageId(task_type),
                                      base::UTF8ToUTF16(script_url.spec())),
           script_url.spec(),
           nullptr /* icon */,
           handle),
      task_type_(task_type),
      render_process_id_(render_process_id) {}

WorkerTask::~WorkerTask() = default;

Task::Type WorkerTask::GetType() const {
  return task_type_;
}

int WorkerTask::GetChildProcessUniqueID() const {
  return render_process_id_;
}

}  // namespace task_manager
