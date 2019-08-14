// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_discovery_filter.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluetooth_common.h"

namespace device {

BluetoothDiscoveryFilter::BluetoothDiscoveryFilter() {
  SetTransport(BluetoothTransport::BLUETOOTH_TRANSPORT_DUAL);
}

BluetoothDiscoveryFilter::BluetoothDiscoveryFilter(
    BluetoothTransport transport) {
  SetTransport(transport);
}

BluetoothDiscoveryFilter::~BluetoothDiscoveryFilter() = default;

bool BluetoothDiscoveryFilter::GetRSSI(int16_t* out_rssi) const {
  DCHECK(out_rssi);
  if (!rssi_)
    return false;

  *out_rssi = *rssi_;
  return true;
}

void BluetoothDiscoveryFilter::SetRSSI(int16_t rssi) {
  rssi_ = rssi;
}

bool BluetoothDiscoveryFilter::GetPathloss(uint16_t* out_pathloss) const {
  DCHECK(out_pathloss);
  if (!pathloss_)
    return false;

  *out_pathloss = *pathloss_;
  return true;
}

void BluetoothDiscoveryFilter::SetPathloss(uint16_t pathloss) {
  pathloss_ = pathloss;
}

BluetoothTransport BluetoothDiscoveryFilter::GetTransport() const {
  return transport_;
}

void BluetoothDiscoveryFilter::SetTransport(BluetoothTransport transport) {
  DCHECK(transport != BLUETOOTH_TRANSPORT_INVALID);
  transport_ = transport;
}

void BluetoothDiscoveryFilter::GetUUIDs(
    std::set<device::BluetoothUUID>& out_uuids) const {
  out_uuids.clear();

  for (const auto& uuid : uuids_)
    out_uuids.insert(*uuid);
}

void BluetoothDiscoveryFilter::AddUUID(const device::BluetoothUUID& uuid) {
  DCHECK(uuid.IsValid());
  for (const auto& uuid_it : uuids_) {
    if (*uuid_it == uuid)
      return;
  }

  uuids_.push_back(std::make_unique<device::BluetoothUUID>(uuid));
}

void BluetoothDiscoveryFilter::CopyFrom(
    const BluetoothDiscoveryFilter& filter) {
  transport_ = filter.transport_;

  uuids_.clear();
  if (filter.uuids_.size()) {
    for (const auto& uuid : filter.uuids_)
      AddUUID(*uuid);
  }

  rssi_ = filter.rssi_;
  pathloss_ = filter.pathloss_;
}

std::unique_ptr<device::BluetoothDiscoveryFilter>
BluetoothDiscoveryFilter::Merge(
    const device::BluetoothDiscoveryFilter* filter_a,
    const device::BluetoothDiscoveryFilter* filter_b) {
  std::unique_ptr<BluetoothDiscoveryFilter> result;

  if (!filter_a && !filter_b) {
    return result;
  }

  result.reset(new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_DUAL));

  if (!filter_a || !filter_b || filter_a->IsDefault() ||
      filter_b->IsDefault()) {
    return result;
  }

  // both filters are not empty, so they must have transport set.
  result->SetTransport(static_cast<BluetoothTransport>(filter_a->transport_ |
                                                       filter_b->transport_));

  // if both filters have uuids, them merge them. Otherwise uuids filter should
  // be left empty
  if (filter_a->uuids_.size() && filter_b->uuids_.size()) {
    std::set<device::BluetoothUUID> uuids;
    filter_a->GetUUIDs(uuids);
    for (auto& uuid : uuids)
      result->AddUUID(uuid);

    filter_b->GetUUIDs(uuids);
    for (auto& uuid : uuids)
      result->AddUUID(uuid);
  }

  if ((filter_a->rssi_ && filter_b->pathloss_) ||
      (filter_a->pathloss_ && filter_b->rssi_)) {
    // if both rssi and pathloss filtering is enabled in two different
    // filters, we can't tell which filter is more generic, and we don't set
    // proximity filtering on merged filter.
    return result;
  }

  if (filter_a->rssi_ && filter_b->rssi_) {
    result->SetRSSI(std::min(*filter_a->rssi_, *filter_b->rssi_));
  } else if (filter_a->pathloss_ && filter_b->pathloss_) {
    result->SetPathloss(std::max(*filter_a->pathloss_, *filter_b->pathloss_));
  }

  return result;
}

bool BluetoothDiscoveryFilter::Equals(
    const BluetoothDiscoveryFilter& other) const {
  if ((rssi_.has_value() != other.rssi_.has_value()) ||
      (rssi_ && other.rssi_ && *rssi_ != *other.rssi_))
    return false;

  if ((pathloss_.has_value() != other.pathloss_.has_value()) ||
      (pathloss_ && other.pathloss_ && *pathloss_ != *other.pathloss_)) {
    return false;
  }

  if (transport_ != other.transport_)
    return false;

  std::set<device::BluetoothUUID> uuids_a, uuids_b;
  GetUUIDs(uuids_a);
  other.GetUUIDs(uuids_b);
  if (uuids_a != uuids_b)
    return false;

  return true;
}

bool BluetoothDiscoveryFilter::IsDefault() const {
  return !(rssi_ || pathloss_ || uuids_.size() ||
           transport_ != BLUETOOTH_TRANSPORT_DUAL);
}

}  // namespace device
