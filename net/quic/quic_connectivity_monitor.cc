// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connectivity_monitor.h"

#include "base/metrics/histogram_macros.h"

namespace net {

QuicConnectivityMonitor::QuicConnectivityMonitor(
    NetworkChangeNotifier::NetworkHandle default_network)
    : default_network_(default_network) {}

QuicConnectivityMonitor::~QuicConnectivityMonitor() = default;

void QuicConnectivityMonitor::RecordConnectivityStatsToHistograms(
    const std::string& notification,
    NetworkChangeNotifier::NetworkHandle affected_network) const {
  if (notification == "OnNetworkSoonToDisconnect" ||
      notification == "OnNetworkDisconnected") {
    // If the disconnected network is not the default network, ignore
    // stats collections.
    if (affected_network != default_network_)
      return;
  }

  // TODO(crbug.com/1090532): rename histograms prefix to
  // Net.QuicConnectivityMonitor.
  UMA_HISTOGRAM_COUNTS_100(
      "Net.QuicStreamFactory.NumQuicSessionsAtNetworkChange",
      active_sessions_.size());

  // Skip degrading session collection if there are less than two sessions.
  if (active_sessions_.size() < 2)
    return;

  size_t num_degrading_sessions = GetNumDegradingSessions();
  const std::string raw_histogram_name =
      "Net.QuicStreamFactory.NumDegradingSessions." + notification;
  base::UmaHistogramExactLinear(raw_histogram_name, num_degrading_sessions,
                                101);

  int percentage = num_degrading_sessions * 100 / active_sessions_.size();
  const std::string percentage_histogram_name =
      "Net.QuicStreamFactory.PercentageDegradingSessions." + notification;
  base::UmaHistogramExactLinear(percentage_histogram_name, percentage, 101);
}

size_t QuicConnectivityMonitor::GetNumDegradingSessions() const {
  return degrading_sessions_.size();
}

size_t QuicConnectivityMonitor::GetCountForWriteErrorCode(
    int write_error_code) const {
  auto it = write_error_map_.find(write_error_code);
  return it == write_error_map_.end() ? 0u : it->second;
}

void QuicConnectivityMonitor::SetInitialDefaultNetwork(
    NetworkChangeNotifier::NetworkHandle default_network) {
  default_network_ = default_network;
}

void QuicConnectivityMonitor::OnSessionPathDegrading(
    QuicChromiumClientSession* session,
    NetworkChangeNotifier::NetworkHandle network) {
  if (network == default_network_)
    degrading_sessions_.insert(session);
}

void QuicConnectivityMonitor::OnSessionResumedPostPathDegrading(
    QuicChromiumClientSession* session,
    NetworkChangeNotifier::NetworkHandle network) {
  if (network == default_network_)
    degrading_sessions_.erase(session);
}

void QuicConnectivityMonitor::OnSessionEncounteringWriteError(
    QuicChromiumClientSession* session,
    NetworkChangeNotifier::NetworkHandle network,
    int error_code) {
  if (network == default_network_)
    ++write_error_map_[error_code];
}

void QuicConnectivityMonitor::OnSessionClosedAfterHandshake(
    QuicChromiumClientSession* session,
    NetworkChangeNotifier::NetworkHandle network,
    quic::ConnectionCloseSource source,
    quic::QuicErrorCode error_code) {
  if (network != default_network_)
    return;

  if (source == quic::ConnectionCloseSource::FROM_PEER) {
    // Connection closed by the peer post handshake with PUBLIC RESET
    // is most likely a NAT rebinding issue.
    if (error_code == quic::QUIC_PUBLIC_RESET)
      quic_error_map_[error_code]++;
    return;
  }

  // Connection close by self with PACKET_WRITE_ERROR or TOO_MANY_RTOS
  // is likely a connectivity issue.
  if (error_code == quic::QUIC_PACKET_WRITE_ERROR ||
      error_code == quic::QUIC_TOO_MANY_RTOS) {
    quic_error_map_[error_code]++;
  }
}

void QuicConnectivityMonitor::OnSessionRegistered(
    QuicChromiumClientSession* session,
    NetworkChangeNotifier::NetworkHandle network) {
  if (network == default_network_) {
    active_sessions_.insert(session);
    total_num_sessions_tracked_++;
  }
}

void QuicConnectivityMonitor::OnSessionRemoved(
    QuicChromiumClientSession* session) {
  degrading_sessions_.erase(session);
  active_sessions_.erase(session);
}

void QuicConnectivityMonitor::OnDefaultNetworkUpdated(
    NetworkChangeNotifier::NetworkHandle default_network) {
  default_network_ = default_network;
  active_sessions_.clear();
  total_num_sessions_tracked_ = 0u;
  degrading_sessions_.clear();
  write_error_map_.clear();
  quic_error_map_.clear();
}

void QuicConnectivityMonitor::OnIPAddressChanged() {
  // If NetworkHandle is supported, connectivity monitor will receive
  // notifications via OnDefaultNetworkUpdated.
  if (NetworkChangeNotifier::AreNetworkHandlesSupported())
    return;

  DCHECK_EQ(default_network_, NetworkChangeNotifier::kInvalidNetworkHandle);
  degrading_sessions_.clear();
  write_error_map_.clear();
}

void QuicConnectivityMonitor::OnSessionGoingAwayOnIPAddressChange(
    QuicChromiumClientSession* session) {
  // This should only be called after ConnectivityMonitor gets notified via
  // OnIPAddressChanged().
  DCHECK(degrading_sessions_.empty());
  // |session| that encounters IP address change will lose track which network
  // it is bound to. Future connectivity monitoring may be misleading.
  session->RemoveConnectivityObserver(this);
}

}  // namespace net
