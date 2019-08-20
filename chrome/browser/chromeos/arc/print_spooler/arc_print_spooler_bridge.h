// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_PRINT_SPOOLER_ARC_PRINT_SPOOLER_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_PRINT_SPOOLER_ARC_PRINT_SPOOLER_BRIDGE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/mojom/print_spooler.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles print related IPC from the ARC container and allows print
// jobs to be displayed and managed in Chrome print preview instead of the
// Android print UI.
class ArcPrintSpoolerBridge
    : public KeyedService,
      public mojom::PrintSpoolerHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcPrintSpoolerBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcPrintSpoolerBridge(content::BrowserContext* context,
                        ArcBridgeService* bridge_service);
  ~ArcPrintSpoolerBridge() override;

  // mojom::PrintSpoolerHost overrides:
  // TODO(jschettler): Add overrides.

 private:
  Profile* const profile_;

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  base::WeakPtrFactory<ArcPrintSpoolerBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcPrintSpoolerBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_PRINT_SPOOLER_ARC_PRINT_SPOOLER_BRIDGE_H_
