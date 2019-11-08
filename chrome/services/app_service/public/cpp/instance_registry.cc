// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/instance_registry.h"

#include <memory>
#include <utility>

#include "chrome/services/app_service/public/cpp/instance.h"
#include "chrome/services/app_service/public/cpp/instance_update.h"

namespace apps {

InstanceRegistry::Observer::Observer(InstanceRegistry* instance_registry) {
  Observe(instance_registry);
}

InstanceRegistry::Observer::Observer() = default;
InstanceRegistry::Observer::~Observer() {
  if (instance_registry_) {
    instance_registry_->RemoveObserver(this);
  }
}

void InstanceRegistry::Observer::Observe(InstanceRegistry* instance_registry) {
  if (instance_registry == instance_registry_) {
    return;
  }

  if (instance_registry_) {
    instance_registry_->RemoveObserver(this);
  }

  instance_registry_ = instance_registry;
  if (instance_registry_) {
    instance_registry_->AddObserver(this);
  }
}

InstanceRegistry::InstanceRegistry() = default;

InstanceRegistry::~InstanceRegistry() {
  for (auto& obs : observers_) {
    obs.OnInstanceRegistryWillBeDestroyed(this);
  }
  DCHECK(!observers_.might_have_observers());
}

void InstanceRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InstanceRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InstanceRegistry::OnInstances(const Instances& deltas) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (!deltas_in_progress_.empty()) {
    for (auto& delta : deltas) {
      deltas_pending_.push_back(delta.get()->Clone());
    }
    return;
  }
  DoOnInstances(std::move(deltas));
  while (!deltas_pending_.empty()) {
    Instances pending;
    pending.swap(deltas_pending_);
    DoOnInstances(std::move(pending));
  }
}

void InstanceRegistry::DoOnInstances(const Instances& deltas) {
  // Merge any deltas elements that have the same window. If an observer's
  // OnInstanceUpdate calls back into this InstanceRegistry, we can present a
  // single delta for any given window.
  for (auto& delta : deltas) {
    auto d_iter = deltas_in_progress_.find(delta->Window());
    if (d_iter != deltas_in_progress_.end()) {
      InstanceUpdate::Merge(d_iter->second, delta.get());
    } else {
      deltas_in_progress_[delta->Window()] = delta.get();
    }
  }

  // The remaining for loops range over the deltas_in_progress_ map, not the
  // deltas vector, so that OninstanceUpdate is called only once per unique
  // window. Notify the observers for every de-duplicated delta.
  for (const auto& d_iter : deltas_in_progress_) {
    auto s_iter = states_.find(d_iter.first);
    Instance* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
    Instance* delta = d_iter.second;

    for (auto& obs : observers_) {
      obs.OnInstanceUpdate(InstanceUpdate(state, delta));
    }
  }

  // Update the states for every de-duplicated delta.
  for (const auto& d_iter : deltas_in_progress_) {
    auto s_iter = states_.find(d_iter.first);
    Instance* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
    Instance* delta = d_iter.second;

    if (state) {
      InstanceUpdate::Merge(state, delta);
    } else {
      states_.insert(std::make_pair(delta->Window(), (delta->Clone())));
    }
  }
  deltas_in_progress_.clear();
}

}  // namespace apps
