// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/print_job_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/post_task.h"
#include "chrome/browser/printing/printer_query.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/child_process_host.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"

namespace extensions {

namespace {

using PrinterQueryCallback =
    base::OnceCallback<void(std::unique_ptr<printing::PrinterQuery>)>;

// Send initialized PrinterQuery to UI thread.
void OnSettingsSetOnIOThread(std::unique_ptr<printing::PrinterQuery> query,
                             PrinterQueryCallback callback) {
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(std::move(callback), std::move(query)));
}

void CreateQueryOnIOThread(std::unique_ptr<printing::PrintSettings> settings,
                           PrinterQueryCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto query = std::make_unique<printing::PrinterQuery>(
      content::ChildProcessHost::kInvalidUniqueID,
      content::ChildProcessHost::kInvalidUniqueID);
  auto* query_ptr = query.get();
  query_ptr->SetSettingsFromPOD(
      std::move(settings),
      base::BindOnce(&OnSettingsSetOnIOThread, std::move(query),
                     std::move(callback)));
}

}  // namespace

PrintJobController::JobState::JobState(scoped_refptr<printing::PrintJob> job,
                                       StartPrintJobCallback callback)
    : job(job), callback(std::move(callback)) {}

PrintJobController::JobState::JobState(JobState&&) = default;

PrintJobController::JobState& PrintJobController::JobState::operator=(
    JobState&&) = default;

PrintJobController::JobState::~JobState() = default;

PrintJobController::PrintJobController() = default;

PrintJobController::~PrintJobController() = default;

void PrintJobController::StartPrintJob(
    const std::string& extension_id,
    std::unique_ptr<printing::MetafileSkia> metafile,
    std::unique_ptr<printing::PrintSettings> settings,
    StartPrintJobCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &CreateQueryOnIOThread, std::move(settings),
          base::BindOnce(&PrintJobController::StartPrinting,
                         weak_ptr_factory_.GetWeakPtr(), extension_id,
                         std::move(metafile), std::move(callback))));
}

void PrintJobController::StartPrinting(
    const std::string& extension_id,
    std::unique_ptr<printing::MetafileSkia> metafile,
    StartPrintJobCallback callback,
    std::unique_ptr<printing::PrinterQuery> query) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto job = base::MakeRefCounted<printing::PrintJob>();
  // Save in separate variable because |query| is moved.
  base::string16 title = query->settings().title();
  job->Initialize(std::move(query), title, /*page_count=*/1);
  job->SetSource(printing::PrintJob::Source::EXTENSION, extension_id);
  printing::PrintedDocument* document = job->document();
  // |paper_size| and |page_rect| are used only for OS_MACOSX, so just use
  // default constructor values.
  document->SetDocument(std::move(metafile),
                        /*paper_size=*/gfx::Size(),
                        /*page_rect=*/gfx::Rect());
  // Save PrintJob scoped refptr and callback to resolve when print job is
  // created.
  extension_pending_jobs_[extension_id].emplace(job, std::move(callback));
  job->StartPrinting();
}

void PrintJobController::OnPrintJobCreated(const std::string& extension_id,
                                           const std::string& job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = extension_pending_jobs_.find(extension_id);
  if (it == extension_pending_jobs_.end())
    return;
  auto& pending_jobs = it->second;
  if (!pending_jobs.empty()) {
    // We need to move corresponding scoped refptr of PrintJob from queue
    // of pending pointers to global map. We can't drop it as the print job is
    // not completed yet so we should not destruct it.
    print_jobs_map_[job_id] = pending_jobs.front().job;
    // The job is submitted to CUPS so we have to resolve the first callback
    // in the corresponding queue.
    auto callback = std::move(pending_jobs.front().callback);
    pending_jobs.pop();
    if (pending_jobs.empty())
      extension_pending_jobs_.erase(it);
    std::move(callback).Run(std::make_unique<std::string>(job_id));
  }
}

void PrintJobController::OnPrintJobFinished(const std::string& job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  print_jobs_map_.erase(job_id);
}

}  // namespace extensions
