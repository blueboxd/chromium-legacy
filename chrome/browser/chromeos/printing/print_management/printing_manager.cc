// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_management/printing_manager.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/chromeos/printing/print_management/print_job_info_mojom_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {
namespace printing {
namespace print_management {

PrintingManager::PrintingManager(Profile* profile) : profile_(profile) {}

PrintingManager::~PrintingManager() = default;

void PrintingManager::GetPrintJobs(GetPrintJobsCallback callback) {
  chromeos::PrintJobHistoryService* print_job_history_service =
      chromeos::PrintJobHistoryServiceFactory::GetForBrowserContext(profile_);
  print_job_history_service->GetPrintJobs(
      base::BindOnce(&PrintingManager::OnPrintJobsRetrieved,
                     base::Unretained(this), std::move(callback)));
}

void PrintingManager::OnPrintJobsRetrieved(
    GetPrintJobsCallback callback,
    bool success,
    std::unique_ptr<std::vector<chromeos::printing::proto::PrintJobInfo>>
        print_job_info_protos) {
  std::vector<mojom::PrintJobInfoPtr> print_job_infos;

  if (success) {
    DCHECK(print_job_info_protos);
    for (const auto& print_job_info : *print_job_info_protos) {
      print_job_infos.push_back(PrintJobProtoToMojom(print_job_info));
    }
  }

  std::move(callback).Run(std::move(print_job_infos));
}

void PrintingManager::BindInterface(
    mojo::PendingReceiver<mojom::PrintingMetadataProvider> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void PrintingManager::Shutdown() {
  receiver_.reset();
}

}  // namespace print_management
}  // namespace printing
}  // namespace chromeos
