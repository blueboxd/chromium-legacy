// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_WEBRTC_SHARING_SERVICE_HOST_H_
#define CHROME_BROWSER_SHARING_WEBRTC_SHARING_SERVICE_HOST_H_

#include <string>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_message_sender.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "chrome/browser/sharing/webrtc/ice_config_fetcher.h"
#include "chrome/services/sharing/public/mojom/sharing.mojom.h"
#include "chrome/services/sharing/public/mojom/webrtc.mojom.h"
#include "components/sync_device_info/device_info.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class SharingHandlerRegistry;
class SharingWebRtcConnectionHost;
struct SharingWebRtcMojoPipes;

// Connects to the Sharing service running in a sandboxed process and manages
// active WebRTC connections. This object is owned by the |message_sender|.
class SharingServiceHost : public SharingMessageSender::SendMessageDelegate {
 public:
  using SendMessageCallback =
      base::OnceCallback<void(SharingSendMessageResult,
                              base::Optional<std::string>)>;

  SharingServiceHost(
      SharingMessageSender* message_sender,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  SharingServiceHost(const SharingServiceHost&) = delete;
  SharingServiceHost& operator=(const SharingServiceHost&) = delete;
  ~SharingServiceHost() override;

  // SharingMessageSender::SendMessageDelegate:
  void DoSendMessageToDevice(const syncer::DeviceInfo& device,
                             base::TimeDelta time_to_live,
                             chrome_browser_sharing::SharingMessage message,
                             SendMessageCallback callback) override;

  void OnOfferReceived(const std::string& device_guid,
                       const syncer::DeviceInfo::SharingTargetInfo& target_info,
                       const std::string& offer,
                       base::OnceCallback<void(const std::string&)> callback);

  void OnIceCandidatesReceived(
      const std::string& device_guid,
      const syncer::DeviceInfo::SharingTargetInfo& target_info,
      std::vector<sharing::mojom::IceCandidatePtr> ice_candidates);

  void SetSharingHandlerRegistry(SharingHandlerRegistry* handler_registry);

 private:
  void OnPeerConnectionClosed(const std::string& device_guid);

  SharingWebRtcConnectionHost* GetConnection(
      const std::string& device_guid,
      const syncer::DeviceInfo::SharingTargetInfo& target_info);

  void OnIceServersReceived(
      std::unique_ptr<SharingWebRtcMojoPipes> pipes,
      std::vector<sharing::mojom::IceServerPtr> ice_servers);

  network::mojom::NetworkContext* GetNetworkContext();

  // Owned by the SharingService KeyedService and owns |this|.
  SharingMessageSender* message_sender_;
  IceConfigFetcher ice_config_fetcher_;

  mojo::Remote<sharing::mojom::Sharing> sharing_utility_service_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;

  // Map of device_guid to SharingWebRtcConnectionHost containing all currently
  // active connections.
  std::unordered_map<std::string, std::unique_ptr<SharingWebRtcConnectionHost>>
      connections_;

  // Will be set when a message handler for this is registered. Owned by the
  // SharingService KeyedService.
  SharingHandlerRegistry* handler_registry_ = nullptr;

  base::WeakPtrFactory<SharingServiceHost> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_WEBRTC_SHARING_SERVICE_HOST_H_
