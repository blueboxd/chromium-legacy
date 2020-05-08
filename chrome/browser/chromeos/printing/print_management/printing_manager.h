// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_H_

#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
class PrintJobHistoryService;
namespace printing {
namespace print_management {

class PrintingManager
    : public printing_manager::mojom::PrintingMetadataProvider,
      public KeyedService {
 public:
  explicit PrintingManager(PrintJobHistoryService* print_job_history_service);
  ~PrintingManager() override;

  PrintingManager(const PrintingManager&) = delete;
  PrintingManager& operator=(const PrintingManager&) = delete;

  // printing_manager::mojom::PrintingMetadataProvider:
  void GetPrintJobs(GetPrintJobsCallback callback) override;
  void DeleteAllPrintJobs(DeleteAllPrintJobsCallback callback) override;

  void BindInterface(
      mojo::PendingReceiver<printing_manager::mojom::PrintingMetadataProvider>
          pending_receiver);

 private:
  // KeyedService:
  void Shutdown() override;

  void OnPrintJobsRetrieved(GetPrintJobsCallback callback,
                            bool success,
                            std::vector<chromeos::printing::proto::PrintJobInfo>
                                print_job_info_protos);

  mojo::Receiver<printing_manager::mojom::PrintingMetadataProvider> receiver_{
      this};
  chromeos::PrintJobHistoryService* print_job_history_service_;  // NOT OWNED.
};

}  // namespace print_management
}  // namespace printing
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_H_
