// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_management/printing_manager.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/chromeos/printing/print_management/print_job_info_mojom_conversions.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {
namespace printing {
namespace print_management {

using chromeos::printing::proto::PrintJobInfo;
using printing_manager::mojom::PrintingMetadataProvider;
using printing_manager::mojom::PrintJobInfoPtr;

PrintingManager::PrintingManager(
    PrintJobHistoryService* print_job_history_service)
    : print_job_history_service_(print_job_history_service) {}

PrintingManager::~PrintingManager() = default;

void PrintingManager::GetPrintJobs(GetPrintJobsCallback callback) {
  print_job_history_service_->GetPrintJobs(
      base::BindOnce(&PrintingManager::OnPrintJobsRetrieved,
                     base::Unretained(this), std::move(callback)));
}

void PrintingManager::DeleteAllPrintJobs(DeleteAllPrintJobsCallback callback) {
  print_job_history_service_->DeleteAllPrintJobs(std::move(callback));
}

void PrintingManager::OnPrintJobsRetrieved(
    GetPrintJobsCallback callback,
    bool success,
    std::vector<PrintJobInfo> print_job_info_protos) {
  std::vector<PrintJobInfoPtr> print_job_infos;

  if (success) {
    for (const auto& print_job_info : print_job_info_protos) {
      print_job_infos.push_back(PrintJobProtoToMojom(print_job_info));
    }
  }

  std::move(callback).Run(std::move(print_job_infos));
}

void PrintingManager::BindInterface(
    mojo::PendingReceiver<PrintingMetadataProvider> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void PrintingManager::Shutdown() {
  receiver_.reset();
}

}  // namespace print_management
}  // namespace printing
}  // namespace chromeos
