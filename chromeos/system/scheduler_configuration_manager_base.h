// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_SCHEDULER_CONFIGURATION_MANAGER_BASE_H_
#define CHROMEOS_SYSTEM_SCHEDULER_CONFIGURATION_MANAGER_BASE_H_

#include <stddef.h>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace chromeos {

// A base class for SchedulerConfigurationManager.
class COMPONENT_EXPORT(CHROMEOS_SYSTEM) SchedulerConfigurationManagerBase {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when SetSchedulerConfiguration D-Bus call to debugd returns.
    virtual void OnConfigurationSet(bool success,
                                    size_t num_cores_disabled) = 0;
  };

  SchedulerConfigurationManagerBase();
  virtual ~SchedulerConfigurationManagerBase();

  void AddObserver(Observer* obs);
  void RemoveObserver(const Observer* obs);

 protected:
  base::ObserverList<Observer> observer_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerConfigurationManagerBase);
};

}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_SCHEDULER_CONFIGURATION_MANAGER_BASE_H_
