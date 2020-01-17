// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINT_JOB_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINT_JOB_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/printing/print_job.h"

namespace printing {
class MetafileSkia;
class PrinterQuery;
class PrintSettings;
}  // namespace printing

namespace extensions {

// This class is responsible for sending print jobs in the printing pipeline and
// cancelling them. It should be used by API handler as the entry point of
// actual printing pipeline.
// This class lives on UI thread.
class PrintJobController {
 public:
  using StartPrintJobCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> job_id)>;

  PrintJobController();
  ~PrintJobController();

  // Creates, initializes and adds print job to the queue of pending print jobs.
  void StartPrintJob(const std::string& extension_id,
                     std::unique_ptr<printing::MetafileSkia> metafile,
                     std::unique_ptr<printing::PrintSettings> settings,
                     StartPrintJobCallback callback);

  // Moves print job pointer to |print_jobs_map_| and resolves corresponding
  // callback.
  // This should be called when CupsPrintJobManager created CupsPrintJob.
  void OnPrintJobCreated(const std::string& extension_id,
                         const std::string& job_id);

  // Removes print job pointer from |print_jobs_map_| as the job is finished.
  // This should be called when CupsPrintJob is finished (it could be either
  // completed, failed or cancelled).
  void OnPrintJobFinished(const std::string& job_id);

 private:
  struct JobState {
    scoped_refptr<printing::PrintJob> job;
    StartPrintJobCallback callback;
    JobState(scoped_refptr<printing::PrintJob> job,
             StartPrintJobCallback callback);
    JobState(JobState&&);
    JobState& operator=(JobState&&);
    ~JobState();
  };

  void StartPrinting(const std::string& extension_id,
                     std::unique_ptr<printing::MetafileSkia> metafile,
                     StartPrintJobCallback callback,
                     std::unique_ptr<printing::PrinterQuery> query);

  // Stores mapping from extension id to queue of pending jobs to resolve.
  // Placing a job state in the map means that we sent print job to the printing
  // pipeline and have been waiting for the response with created job id.
  // After that we could resolve a callback and move PrintJob to global map.
  // We need to store job pointers to keep the current scheduled print jobs
  // alive (as they're ref counted).
  base::flat_map<std::string, base::queue<JobState>> extension_pending_jobs_;

  // Stores mapping from job id to printing::PrintJob.
  // This is needed to hold PrintJob pointer and correct handle CancelJob()
  // requests.
  base::flat_map<std::string, scoped_refptr<printing::PrintJob>>
      print_jobs_map_;

  base::WeakPtrFactory<PrintJobController> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINT_JOB_CONTROLLER_H_
