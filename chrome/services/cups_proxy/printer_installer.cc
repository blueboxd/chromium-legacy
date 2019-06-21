// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/printer_installer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/services/cups_proxy/public/cpp/cups_util.h"
#include "chromeos/printing/printer_configuration.h"

namespace cups_proxy {
namespace {

using CupsProxyServiceDelegate = chromeos::printing::CupsProxyServiceDelegate;

class PrinterInstallerImpl : public PrinterInstaller {
 public:
  explicit PrinterInstallerImpl(
      base::WeakPtr<CupsProxyServiceDelegate> delegate);
  ~PrinterInstallerImpl() override = default;

  void InstallPrinterIfNeeded(ipp_t* ipp, InstallPrinterCallback cb) override;

 private:
  void OnInstallPrinter(InstallPrinterCallback cb, bool success);

  void Finish(InstallPrinterCallback cb, InstallPrinterResult res);

  // Service delegate granting access to printing stack dependencies.
  base::WeakPtr<CupsProxyServiceDelegate> delegate_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PrinterInstallerImpl> weak_factory_;
};

}  // namespace

PrinterInstallerImpl::PrinterInstallerImpl(
    base::WeakPtr<CupsProxyServiceDelegate> delegate)
    : delegate_(std::move(delegate)), weak_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void PrinterInstallerImpl::InstallPrinterIfNeeded(ipp_t* ipp,
                                                  InstallPrinterCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto printer_uuid = GetPrinterId(ipp);
  if (!printer_uuid) {
    Finish(std::move(cb), InstallPrinterResult::kNoPrintersFound);
    return;
  }

  auto printer = delegate_->GetPrinter(*printer_uuid);
  if (!printer) {
    // If the requested printer DNE, we proxy to CUPSd and allow it to
    // handle the error.
    Finish(std::move(cb), InstallPrinterResult::kUnknownPrinterFound);
    return;
  }

  if (delegate_->IsPrinterInstalled(*printer)) {
    Finish(std::move(cb), InstallPrinterResult::kSuccess);
    return;
  }

  // Install printer.
  delegate_->SetupPrinter(
      *printer, base::BindOnce(&PrinterInstallerImpl::OnInstallPrinter,
                               weak_factory_.GetWeakPtr(), std::move(cb)));
}

// TODO(crbug.com/945409): Test whether we need to call
// CupsPrintersManager::PrinterInstalled here.
void PrinterInstallerImpl::OnInstallPrinter(InstallPrinterCallback cb,
                                            bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Finish(std::move(cb),
         success ? InstallPrinterResult::kSuccess
                 : InstallPrinterResult::kPrinterInstallationFailure);
}

void PrinterInstallerImpl::Finish(InstallPrinterCallback cb,
                                  InstallPrinterResult res) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), res));
}

// static
std::unique_ptr<PrinterInstaller> PrinterInstaller::Create(
    base::WeakPtr<CupsProxyServiceDelegate> delegate) {
  return std::make_unique<PrinterInstallerImpl>(std::move(delegate));
}

}  // namespace cups_proxy
