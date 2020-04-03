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

class Profile;

namespace chromeos {
namespace printing {
namespace mojom = printing_manager::mojom;
namespace print_management {

class PrintingManager : public mojom::PrintingMetadataProvider,
                        public KeyedService {
 public:
  explicit PrintingManager(Profile* profile);
  ~PrintingManager() override;

  PrintingManager(const PrintingManager&) = delete;
  PrintingManager& operator=(const PrintingManager&) = delete;

  // mojom::PrintingMetadataProvider:
  void GetPrintJobs(GetPrintJobsCallback callback) override;

  void BindInterface(
      mojo::PendingReceiver<mojom::PrintingMetadataProvider> pending_receiver);

 private:
  // KeyedService:
  void Shutdown() override;

  void OnPrintJobsRetrieved(
      GetPrintJobsCallback callback,
      bool success,
      std::unique_ptr<std::vector<chromeos::printing::proto::PrintJobInfo>>
          print_job_info_protos);

  mojo::Receiver<mojom::PrintingMetadataProvider> receiver_{this};

  Profile* profile_;  // Not Owned.
};

}  // namespace print_management
}  // namespace printing
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_H_
