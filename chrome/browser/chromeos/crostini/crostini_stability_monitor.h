// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_STABILITY_MONITOR_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_STABILITY_MONITOR_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chromeos/dbus/chunneld_client.h"
#include "chromeos/dbus/cicerone_client.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/seneschal_client.h"

namespace crostini {

extern const char kCrostiniStabilityHistogram[];

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// CrostiniFailureClasses in src/tools/metrics/histograms/enums.xml and the copy
// in src/platform2/vm_tools/cicerone/crash_listener_impl.cc
enum class FailureClasses {
  ConciergeStopped = 0,
  CiceroneStopped = 1,
  SeneschalStopped = 2,
  ChunneldStopped = 3,
  VmStopped = 4,
  VmSyslogStopped = 5,
  VshdStopped = 6,
  LxcFsStopped = 7,
  TremplinStopped = 8,
  NdproxydStopped = 9,
  McastdStopped = 10,
  LxdStopped = 11,
  GarconStopped = 12,
  SommelierStopped = 13,
  SommelierXStopped = 14,
  CrosSftpStopped = 15,
  CrosNotificationdStopped = 16,
  kMaxValue = CrosNotificationdStopped,
};

class CrostiniStabilityMonitor : chromeos::ConciergeClient::Observer,
                                 chromeos::CiceroneClient::Observer,
                                 chromeos::SeneschalClient::Observer,
                                 chromeos::ChunneldClient::Observer,
                                 VmShutdownObserver {
 public:
  explicit CrostiniStabilityMonitor(CrostiniManager* crostini_manager);
  ~CrostiniStabilityMonitor() override;

  CrostiniStabilityMonitor(const CrostiniStabilityMonitor&) = delete;
  CrostiniStabilityMonitor& operator=(const CrostiniStabilityMonitor&) = delete;

  void ConciergeStarted(bool is_available);
  void CiceroneStarted(bool is_available);
  void SeneschalStarted(bool is_available);
  void ChunneldStarted(bool is_available);

  //  chromeos::ConciergeClient::Observer::
  void ConciergeServiceStopped() override;
  void ConciergeServiceStarted() override;

  //  chromeos::CiceroneClient::Observer::
  void CiceroneServiceStopped() override;
  void CiceroneServiceStarted() override;

  //  chromeos::SeneschalClient::Observer::
  void SeneschalServiceStopped() override;
  void SeneschalServiceStarted() override;

  //  chromeos::ChunneldClient::Observer::
  void ChunneldServiceStopped() override;
  void ChunneldServiceStarted() override;

  //  VmShutdownObserver::
  void OnVmShutdown(const std::string& vm_name) override;

 private:
  ScopedObserver<chromeos::ConciergeClient, chromeos::ConciergeClient::Observer>
      concierge_observer_;
  ScopedObserver<chromeos::CiceroneClient, chromeos::CiceroneClient::Observer>
      cicerone_observer_;
  ScopedObserver<chromeos::SeneschalClient, chromeos::SeneschalClient::Observer>
      seneschal_observer_;
  ScopedObserver<chromeos::ChunneldClient, chromeos::ChunneldClient::Observer>
      chunneld_observer_;
  ScopedObserver<CrostiniManager,
                 VmShutdownObserver,
                 &CrostiniManager::AddVmShutdownObserver,
                 &CrostiniManager::RemoveVmShutdownObserver>
      vm_stopped_observer_;

  base::WeakPtr<CrostiniManager> crostini_manager_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrostiniStabilityMonitor> weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_STABILITY_MONITOR_H_
