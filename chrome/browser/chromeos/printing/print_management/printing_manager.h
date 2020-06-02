// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
class DeletionInfo;
class HistoryService;
class PrintJobHistoryService;
namespace printing {
namespace print_management {

class PrintingManager
    : public printing_manager::mojom::PrintingMetadataProvider,
      public KeyedService,
      public history::HistoryServiceObserver {
 public:
  PrintingManager(PrintJobHistoryService* print_job_history_service,
                  history::HistoryService* history_service);

  ~PrintingManager() override;

  PrintingManager(const PrintingManager&) = delete;
  PrintingManager& operator=(const PrintingManager&) = delete;

  // printing_manager::mojom::PrintingMetadataProvider:
  void GetPrintJobs(GetPrintJobsCallback callback) override;
  void DeleteAllPrintJobs(DeleteAllPrintJobsCallback callback) override;
  void ObservePrintJobs(
      mojo::PendingRemote<printing_manager::mojom::PrintJobsObserver> observer,
      ObservePrintJobsCallback callback) override;

  void BindInterface(
      mojo::PendingReceiver<printing_manager::mojom::PrintingMetadataProvider>
          pending_receiver);

 private:
  // KeyedService:
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  void OnPrintJobsRetrieved(GetPrintJobsCallback callback,
                            bool success,
                            std::vector<chromeos::printing::proto::PrintJobInfo>
                                print_job_info_protos);

  // Callback function that is called when the print jobs are cleared from the
  // local database.
  void OnPrintJobsDeleted(bool success);

  // Returns true if the policy pref is enabled to prevent history deletions.
  // TODO(crbug/1053704): Add the policy pref and implement this function.
  bool IsHistoryDeletionPreventedByPolicy();

  // Set of PrintJobsObserver mojom::remotes, each remote is bound to a
  // renderer process receiver. Automatically handles removing disconnected
  // receivers.
  mojo::RemoteSet<printing_manager::mojom::PrintJobsObserver>
      print_job_observers_;

  mojo::Receiver<printing_manager::mojom::PrintingMetadataProvider> receiver_{
      this};

  // Not owned, this is the intermediate layer to interact with the print
  // job local database.
  chromeos::PrintJobHistoryService* print_job_history_service_;

  // Not owned, this provides the necessary observers to observe when browser
  // history has been cleared.
  history::HistoryService* history_service_;

  base::WeakPtrFactory<PrintingManager> weak_ptr_factory_{this};
};

}  // namespace print_management
}  // namespace printing
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_H_
