// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_PRINT_SPOOLER_PRINT_SESSION_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_PRINT_SPOOLER_PRINT_SESSION_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/ash/arc_custom_tab_modal_dialog_host.h"
#include "components/arc/mojom/print_spooler.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {
class ArcCustomTab;
}  // namespace ash

namespace content {
class WebContents;
}  // namespace content

// Implementation of PrintSession interface. Allows ARC to manage and interact
// with the ARC Custom Tab used for printing.
class PrintSessionImpl : public arc::mojom::PrintSession,
                         public ArcCustomTabModalDialogHost {
 public:
  static arc::mojom::PrintSessionPtr Create(
      std::unique_ptr<content::WebContents> web_contents,
      std::unique_ptr<ash::ArcCustomTab> custom_tab,
      arc::mojom::PrintRendererDelegatePtr delegate);

 private:
  PrintSessionImpl(std::unique_ptr<content::WebContents> web_contents,
                   std::unique_ptr<ash::ArcCustomTab> custom_tab,
                   arc::mojom::PrintRendererDelegatePtr delegate,
                   arc::mojom::PrintSessionRequest request);
  ~PrintSessionImpl() override;

  void Bind(arc::mojom::PrintSessionPtr* ptr);

  // Used to close the ARC Custom Tab used for printing. If the remote end
  // closes the connection, the ARC Custom Tab and print preview will be closed.
  // If printing has already started, this will not cancel any active print job.
  void Close();

  // Opens Chrome print preview and hands the PrintRendererDelegate interface
  // pointer to the PrintRenderer.
  void StartPrintAfterDelay(arc::mojom::PrintRendererDelegatePtr delegate);

  // Used to bind the PrintSession interface implementation to a message pipe.
  mojo::Binding<arc::mojom::PrintSession> binding_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<PrintSessionImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrintSessionImpl);
};

#endif  // CHROME_BROWSER_CHROMEOS_ARC_PRINT_SPOOLER_PRINT_SESSION_IMPL_H_
