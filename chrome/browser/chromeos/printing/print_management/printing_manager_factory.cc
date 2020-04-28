// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_management/printing_manager_factory.h"

#include "chrome/browser/chromeos/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/chromeos/printing/print_management/printing_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace printing {
namespace print_management {

// static
PrintingManager* PrintingManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<PrintingManager*>(
      PrintingManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
PrintingManagerFactory* PrintingManagerFactory::GetInstance() {
  return base::Singleton<PrintingManagerFactory>::get();
}

PrintingManagerFactory::PrintingManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PrintingManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(PrintJobHistoryServiceFactory::GetInstance());
}

PrintingManagerFactory::~PrintingManagerFactory() = default;

KeyedService* PrintingManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PrintingManager(
      PrintJobHistoryServiceFactory::GetForBrowserContext(context));
}

}  // namespace print_management
}  // namespace printing
}  // namespace chromeos
