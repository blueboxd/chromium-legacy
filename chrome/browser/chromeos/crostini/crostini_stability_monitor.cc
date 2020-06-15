// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_stability_monitor.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace crostini {

const char kCrostiniStabilityHistogram[] = "Crostini.Stability";

CrostiniStabilityMonitor::CrostiniStabilityMonitor(
    CrostiniManager* crostini_manager)
    : concierge_observer_(this),
      cicerone_observer_(this),
      seneschal_observer_(this),
      chunneld_observer_(this),
      vm_stopped_observer_(this),
      crostini_manager_(crostini_manager->GetWeakPtr()) {
  auto* concierge_client =
      chromeos::DBusThreadManager::Get()->GetConciergeClient();
  DCHECK(concierge_client);
  concierge_client->WaitForServiceToBeAvailable(
      base::BindOnce(&CrostiniStabilityMonitor::ConciergeStarted,
                     weak_ptr_factory_.GetWeakPtr()));

  auto* cicerone_client =
      chromeos::DBusThreadManager::Get()->GetCiceroneClient();
  DCHECK(cicerone_client);
  cicerone_client->WaitForServiceToBeAvailable(
      base::BindOnce(&CrostiniStabilityMonitor::CiceroneStarted,
                     weak_ptr_factory_.GetWeakPtr()));

  auto* seneschal_client =
      chromeos::DBusThreadManager::Get()->GetSeneschalClient();
  DCHECK(seneschal_client);
  seneschal_client->WaitForServiceToBeAvailable(
      base::BindOnce(&CrostiniStabilityMonitor::SeneschalStarted,
                     weak_ptr_factory_.GetWeakPtr()));

  auto* chunneld_client =
      chromeos::DBusThreadManager::Get()->GetChunneldClient();
  DCHECK(chunneld_client);
  chunneld_client->WaitForServiceToBeAvailable(
      base::BindOnce(&CrostiniStabilityMonitor::ChunneldStarted,
                     weak_ptr_factory_.GetWeakPtr()));

  vm_stopped_observer_.Add(crostini_manager);
}

CrostiniStabilityMonitor::~CrostiniStabilityMonitor() {}

void CrostiniStabilityMonitor::ConciergeStarted(bool is_available) {
  DCHECK(is_available);

  auto* concierge_client =
      chromeos::DBusThreadManager::Get()->GetConciergeClient();
  DCHECK(concierge_client);
  concierge_observer_.Add(concierge_client);
}

void CrostiniStabilityMonitor::CiceroneStarted(bool is_available) {
  DCHECK(is_available);

  auto* cicerone_client =
      chromeos::DBusThreadManager::Get()->GetCiceroneClient();
  DCHECK(cicerone_client);
  cicerone_observer_.Add(cicerone_client);
}

void CrostiniStabilityMonitor::SeneschalStarted(bool is_available) {
  DCHECK(is_available);

  auto* seneschal_client =
      chromeos::DBusThreadManager::Get()->GetSeneschalClient();
  DCHECK(seneschal_client);
  seneschal_observer_.Add(seneschal_client);
}

void CrostiniStabilityMonitor::ChunneldStarted(bool is_available) {
  DCHECK(is_available);

  auto* chunneld_client =
      chromeos::DBusThreadManager::Get()->GetChunneldClient();
  DCHECK(chunneld_client);
  chunneld_observer_.Add(chunneld_client);
}

void CrostiniStabilityMonitor::ConciergeServiceStopped() {
  base::UmaHistogramEnumeration(kCrostiniStabilityHistogram,
                                FailureClasses::ConciergeStopped);
}
void CrostiniStabilityMonitor::ConciergeServiceStarted() {}

void CrostiniStabilityMonitor::CiceroneServiceStopped() {
  base::UmaHistogramEnumeration(kCrostiniStabilityHistogram,
                                FailureClasses::CiceroneStopped);
}
void CrostiniStabilityMonitor::CiceroneServiceStarted() {}

void CrostiniStabilityMonitor::SeneschalServiceStopped() {
  base::UmaHistogramEnumeration(kCrostiniStabilityHistogram,
                                FailureClasses::SeneschalStopped);
}
void CrostiniStabilityMonitor::SeneschalServiceStarted() {}

void CrostiniStabilityMonitor::ChunneldServiceStopped() {
  base::UmaHistogramEnumeration(kCrostiniStabilityHistogram,
                                FailureClasses::ChunneldStopped);
}
void CrostiniStabilityMonitor::ChunneldServiceStarted() {}

void CrostiniStabilityMonitor::OnVmShutdown(const std::string& vm_name) {
  // CrostiniManager calls this observer method before removing the VM from its
  // tracking list, so this list will tell us what state the VM was believed to
  // be in before the stop signal was received.
  //
  // If it was STARTING then the error is tracked as a restart failure, not
  // here. If it was STOPPING then the stop was expected and not an error. If it
  // wasn't tracked by CrostiniManager, then we don't care what happens to it.
  //
  // So we can just ask if it was in the STARTED state with ::IsVmRunning.
  if (crostini_manager_->IsVmRunning(vm_name)) {
    base::UmaHistogramEnumeration(kCrostiniStabilityHistogram,
                                  FailureClasses::VmStopped);
  }
}

}  // namespace crostini
