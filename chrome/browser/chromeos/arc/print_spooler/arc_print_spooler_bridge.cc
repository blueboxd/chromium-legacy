// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/print_spooler/arc_print_spooler_bridge.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ui/browser.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"

namespace arc {
namespace {

// Singleton factory for ArcPrintSpoolerBridge.
class ArcPrintSpoolerBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPrintSpoolerBridge, ArcPrintSpoolerBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPrintSpoolerBridgeFactory";

  static ArcPrintSpoolerBridgeFactory* GetInstance() {
    return base::Singleton<ArcPrintSpoolerBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcPrintSpoolerBridgeFactory>;
  ArcPrintSpoolerBridgeFactory() = default;
  ~ArcPrintSpoolerBridgeFactory() override = default;
};

}  // namespace

// static
ArcPrintSpoolerBridge* ArcPrintSpoolerBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ArcPrintSpoolerBridgeFactory::GetForBrowserContext(context);
}

ArcPrintSpoolerBridge::ArcPrintSpoolerBridge(content::BrowserContext* context,
                                             ArcBridgeService* bridge_service)
    : profile_(Profile::FromBrowserContext(context)),
      arc_bridge_service_(bridge_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->print_spooler()->SetHost(this);
}

ArcPrintSpoolerBridge::~ArcPrintSpoolerBridge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->print_spooler()->SetHost(nullptr);
}

}  // namespace arc
