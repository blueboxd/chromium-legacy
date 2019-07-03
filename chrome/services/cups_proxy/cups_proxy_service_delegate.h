// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_CUPS_PROXY_SERVICE_DELEGATE_H_
#define CHROME_SERVICES_CUPS_PROXY_CUPS_PROXY_SERVICE_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/printing/printer_configuration.h"

#include "base/task/post_task.h"

namespace chromeos {
namespace printing {

using PrinterSetupCallback = base::OnceCallback<void(bool)>;

// This delegate grants the CupsProxyService access to the Chrome printing
// stack. This class can be created anywhere but must be accessed from a
// sequenced context.
class CupsProxyServiceDelegate {
 public:
  CupsProxyServiceDelegate();
  virtual ~CupsProxyServiceDelegate();

  // Exposing |weak_factory_|.GetWeakPtr method. Needed to share delegate with
  // CupsProxyService internal managers.
  base::WeakPtr<CupsProxyServiceDelegate> GetWeakPtr();

  virtual std::vector<chromeos::Printer> GetPrinters() = 0;
  virtual base::Optional<chromeos::Printer> GetPrinter(
      const std::string& id) = 0;
  virtual bool IsPrinterInstalled(const Printer& printer) = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() = 0;

  // |cb| will be run on this delegate's sequenced context.
  virtual void SetupPrinter(const Printer& printer,
                            PrinterSetupCallback cb) = 0;

 private:
  base::WeakPtrFactory<CupsProxyServiceDelegate> weak_factory_;
};

}  // namespace printing
}  // namespace chromeos

#endif  // CHROME_SERVICES_CUPS_PROXY_CUPS_PROXY_SERVICE_DELEGATE_H_
