// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_WEBRTC_WEBRTC_SIGNALLING_HOST_FCM_H_
#define CHROME_BROWSER_SHARING_WEBRTC_WEBRTC_SIGNALLING_HOST_FCM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sharing/sharing_message_sender.h"
#include "chrome/services/sharing/public/mojom/webrtc.mojom.h"
#include "components/sync_device_info/device_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// Handles the signalling part of a WebRTC connection via FCM. The signalling
// messages are sent via the |message_sender| to |device_info| through FCM.
class WebRtcSignallingHostFCM : public sharing::mojom::SignallingSender {
 public:
  WebRtcSignallingHostFCM(
      mojo::PendingReceiver<sharing::mojom::SignallingSender> signalling_sender,
      mojo::PendingRemote<sharing::mojom::SignallingReceiver>
          signalling_receiver,
      SharingMessageSender* message_sender,
      std::unique_ptr<syncer::DeviceInfo> device_info);
  WebRtcSignallingHostFCM(const WebRtcSignallingHostFCM&) = delete;
  WebRtcSignallingHostFCM& operator=(const WebRtcSignallingHostFCM&) = delete;
  ~WebRtcSignallingHostFCM() override;

  // sharing::mojom::SignallingSender:
  void SendOffer(const std::string& offer, SendOfferCallback callback) override;
  void SendIceCandidates(
      std::vector<sharing::mojom::IceCandidatePtr> ice_candidates) override;

  virtual void OnOfferReceived(
      const std::string& offer,
      base::OnceCallback<void(const std::string&)> callback);
  virtual void OnIceCandidatesReceived(
      std::vector<sharing::mojom::IceCandidatePtr> ice_candidates);

 private:
  void OnOfferSent(
      SendOfferCallback callback,
      SharingSendMessageResult result,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response);

  SharingMessageSender* message_sender_;
  std::unique_ptr<syncer::DeviceInfo> device_info_;

  mojo::Receiver<sharing::mojom::SignallingSender> signalling_sender_;
  mojo::Remote<sharing::mojom::SignallingReceiver> signalling_receiver_;

  base::WeakPtrFactory<WebRtcSignallingHostFCM> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_WEBRTC_WEBRTC_SIGNALLING_HOST_FCM_H_
