// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/sharing_webrtc_connection.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/webrtc/api/jsep.h"

namespace {

constexpr char kChannelName[] = "chrome-sharing";
// This needs to be less or equal to the WebRTC DataChannel buffer size.
constexpr size_t kMaxQueuedSendDataBytes = 16 * 1024 * 1024;

// A webrtc::SetSessionDescriptionObserver implementation used to receive the
// results of setting local and remote descriptions of the PeerConnection.
class SetSessionDescriptionObserverWrapper
    : public webrtc::SetSessionDescriptionObserver {
 public:
  typedef base::OnceCallback<void(webrtc::RTCError error)> ResultCallback;
  static SetSessionDescriptionObserverWrapper* Create(
      ResultCallback result_callback) {
    return new rtc::RefCountedObject<SetSessionDescriptionObserverWrapper>(
        std::move(result_callback));
  }
  void OnSuccess() override {
    std::move(result_callback_).Run(webrtc::RTCError::OK());
  }
  void OnFailure(webrtc::RTCError error) override {
    std::move(result_callback_).Run(std::move(error));
  }

 protected:
  explicit SetSessionDescriptionObserverWrapper(ResultCallback result_callback)
      : result_callback_(std::move(result_callback)) {}
  ~SetSessionDescriptionObserverWrapper() override = default;

 private:
  ResultCallback result_callback_;
  DISALLOW_COPY_AND_ASSIGN(SetSessionDescriptionObserverWrapper);
};

// A webrtc::CreateSessionDescriptionObserver implementation used to receive the
// results of creating descriptions for this end of the PeerConnection.
class CreateSessionDescriptionObserverWrapper
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  typedef base::OnceCallback<void(
      std::unique_ptr<webrtc::SessionDescriptionInterface> description,
      webrtc::RTCError error)>
      ResultCallback;
  static CreateSessionDescriptionObserverWrapper* Create(
      ResultCallback result_callback) {
    return new rtc::RefCountedObject<CreateSessionDescriptionObserverWrapper>(
        std::move(result_callback));
  }
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    std::move(result_callback_)
        .Run(base::WrapUnique(desc), webrtc::RTCError::OK());
  }
  void OnFailure(webrtc::RTCError error) override {
    std::move(result_callback_).Run(nullptr, std::move(error));
  }

 protected:
  explicit CreateSessionDescriptionObserverWrapper(
      ResultCallback result_callback)
      : result_callback_(std::move(result_callback)) {}
  ~CreateSessionDescriptionObserverWrapper() override = default;

 private:
  ResultCallback result_callback_;
  DISALLOW_COPY_AND_ASSIGN(CreateSessionDescriptionObserverWrapper);
};

}  // namespace

namespace sharing {

SharingWebRtcConnection::SharingWebRtcConnection(
    webrtc::PeerConnectionFactoryInterface* connection_factory,
    const std::vector<mojom::IceServerPtr>& ice_servers,
    mojo::PendingRemote<mojom::SignallingSender> signalling_sender,
    mojo::PendingReceiver<mojom::SignallingReceiver> signalling_receiver,
    mojo::PendingRemote<mojom::SharingWebRtcConnectionDelegate> delegate,
    mojo::PendingReceiver<mojom::SharingWebRtcConnection> connection,
    mojo::PendingRemote<network::mojom::P2PSocketManager> socket_manager,
    mojo::PendingRemote<network::mojom::MdnsResponder> mdns_responder,
    std::unique_ptr<cricket::PortAllocator> port_allocator,
    base::OnceCallback<void(SharingWebRtcConnection*)> on_disconnect)
    : signalling_receiver_(this, std::move(signalling_receiver)),
      signalling_sender_(std::move(signalling_sender)),
      connection_(this, std::move(connection)),
      delegate_(std::move(delegate)),
      on_disconnect_(std::move(on_disconnect)) {
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  for (const auto& ice_server : ice_servers) {
    webrtc::PeerConnectionInterface::IceServer ice_turn_server;
    for (const auto& url : ice_server->urls)
      ice_turn_server.urls.push_back(url.spec());
    if (ice_server->username)
      ice_turn_server.username = *ice_server->username;
    if (ice_server->credential)
      ice_turn_server.password = *ice_server->credential;
    rtc_config.servers.push_back(ice_turn_server);
  }

  signalling_sender_.set_disconnect_handler(
      base::BindOnce(&SharingWebRtcConnection::CloseConnection,
                     weak_ptr_factory_.GetWeakPtr()));
  delegate_.set_disconnect_handler(
      base::BindOnce(&SharingWebRtcConnection::CloseConnection,
                     weak_ptr_factory_.GetWeakPtr()));

  webrtc::PeerConnectionDependencies dependencies(this);

  if (port_allocator)
    dependencies.allocator = std::move(port_allocator);

  // TODO(knollr): Connect to |socket_manager| and |mdns_responder|.

  peer_connection_ = connection_factory->CreatePeerConnection(
      rtc_config, std::move(dependencies));
  CHECK(peer_connection_);
}

SharingWebRtcConnection::~SharingWebRtcConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloseConnection();
  peer_connection_->Close();
}

void SharingWebRtcConnection::OnOfferReceived(
    const std::string& offer,
    OnOfferReceivedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<webrtc::SessionDescriptionInterface> description(
      webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, offer,
                                       nullptr));
  if (!description) {
    LOG(ERROR) << "Failed to parse received offer";
    CloseConnection();
    return;
  }

  peer_connection_->SetRemoteDescription(
      SetSessionDescriptionObserverWrapper::Create(
          base::BindOnce(&SharingWebRtcConnection::CreateAnswer,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))),
      description.release());
}

void SharingWebRtcConnection::OnIceCandidatesReceived(
    std::vector<mojom::IceCandidatePtr> ice_candidates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ice_candidates.empty())
    return;

  // Store received ICE candidates until the signalling state is stable and
  // there is no offer / answer exchange in progress anymore.
  if (!peer_connection_->local_description() ||
      peer_connection_->signaling_state() !=
          webrtc::PeerConnectionInterface::SignalingState::kStable) {
    ice_candidates_.insert(ice_candidates_.end(),
                           std::make_move_iterator(ice_candidates.begin()),
                           std::make_move_iterator(ice_candidates.end()));
    return;
  }

  AddIceCandidates(std::move(ice_candidates));
}

void SharingWebRtcConnection::SendMessage(const std::vector<uint8_t>& message,
                                          SendMessageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (message.empty()) {
    LOG(ERROR) << "Tried to send an empty message";
    std::move(callback).Run(mojom::SendMessageResult::kError);
    return;
  }

  // TODO(crbug.com/1039280): Split / merge logic for messages > 16MB.
  if (message.size() > AvailableBufferSize()) {
    LOG(ERROR) << "Buffer is not large enough";
    std::move(callback).Run(mojom::SendMessageResult::kError);
    return;
  }

  if (!channel_)
    CreateDataChannel();

  if (!channel_) {
    CloseConnection();
    std::move(callback).Run(mojom::SendMessageResult::kError);
    return;
  }

  webrtc::DataBuffer buffer(
      rtc::CopyOnWriteBuffer(message.data(), message.size()), /*binary=*/true);

  // Queue this message until the DataChannel is ready and all queued messages
  // have been sent.
  if (channel_->state() ==
          webrtc::DataChannelInterface::DataState::kConnecting ||
      !queued_messages_.empty()) {
    queued_messages_total_size_ += buffer.size();
    queued_messages_.emplace(std::move(buffer), std::move(callback));
    return;
  }

  if (channel_->state() != webrtc::DataChannelInterface::DataState::kOpen) {
    LOG(ERROR) << "Tried to send while DataChannel was "
               << webrtc::DataChannelInterface::DataStateString(
                      channel_->state());
    std::move(callback).Run(mojom::SendMessageResult::kError);
    return;
  }

  if (!channel_->Send(std::move(buffer))) {
    LOG(ERROR) << "Failed to send message";
    CloseConnection();
    std::move(callback).Run(mojom::SendMessageResult::kError);
    return;
  }

  std::move(callback).Run(mojom::SendMessageResult::kSuccess);
}

void SharingWebRtcConnection::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {}

void SharingWebRtcConnection::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  channel_ = data_channel;
  channel_->RegisterObserver(this);
}

void SharingWebRtcConnection::OnRenegotiationNeeded() {
  CreateOffer();
}

void SharingWebRtcConnection::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {}

void SharingWebRtcConnection::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string candidate_string;
  if (!candidate->ToString(&candidate_string)) {
    LOG(ERROR) << "Failed to serialize IceCandidate";
    return;
  }

  std::vector<mojom::IceCandidatePtr> ice_candidates;
  ice_candidates.push_back(mojom::IceCandidate::New(
      candidate_string, candidate->sdp_mid(), candidate->sdp_mline_index()));

  signalling_sender_->SendIceCandidates(std::move(ice_candidates));
}

void SharingWebRtcConnection::OnStateChange() {
  switch (channel_->state()) {
    case webrtc::DataChannelInterface::DataState::kOpen:
      // Post a task here as we might end up sending a new message which is not
      // allowed from observer callbacks.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&SharingWebRtcConnection::MaybeSendQueuedMessages,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    case webrtc::DataChannelInterface::DataState::kClosed:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&SharingWebRtcConnection::CloseConnection,
                                    weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      break;
  }
}

void SharingWebRtcConnection::OnMessage(const webrtc::DataBuffer& buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<uint8_t> data(buffer.size());
  memcpy(data.data(), buffer.data.cdata(), buffer.size());
  delegate_->OnMessageReceived(data);
}

size_t SharingWebRtcConnection::AvailableBufferSize() const {
  size_t buffered = channel_ ? channel_->buffered_amount() : 0;
  size_t total_used = buffered + queued_messages_total_size_;
  if (total_used > kMaxQueuedSendDataBytes)
    return 0;
  return kMaxQueuedSendDataBytes - total_used;
}

void SharingWebRtcConnection::CreateDataChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  webrtc::DataChannelInit data_channel_init;
  data_channel_init.reliable = true;
  channel_ =
      peer_connection_->CreateDataChannel(kChannelName, &data_channel_init);
  if (channel_)
    channel_->RegisterObserver(this);
  else
    LOG(ERROR) << "Failed to create a DataChannel";
}

void SharingWebRtcConnection::CreateAnswer(OnOfferReceivedCallback callback,
                                           webrtc::RTCError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!error.ok()) {
    LOG(ERROR) << "Failed to set remote description: " << error.message()
               << " (" << webrtc::ToString(error.type()) << ")";
    CloseConnection();
    return;
  }

  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  peer_connection_->CreateAnswer(
      CreateSessionDescriptionObserverWrapper::Create(
          base::BindOnce(&SharingWebRtcConnection::SetLocalAnswer,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))),
      options);
}

void SharingWebRtcConnection::SetLocalAnswer(
    OnOfferReceivedCallback callback,
    std::unique_ptr<webrtc::SessionDescriptionInterface> description,
    webrtc::RTCError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string sdp;
  if (!error.ok() || !description || !description->ToString(&sdp)) {
    if (!error.ok()) {
      LOG(ERROR) << "Failed to create local answer: " << error.message() << " ("
                 << webrtc::ToString(error.type()) << ")";
    } else {
      LOG(ERROR) << "Failed to serialize local answer";
    }
    CloseConnection();
    return;
  }

  peer_connection_->SetLocalDescription(
      SetSessionDescriptionObserverWrapper::Create(base::BindOnce(
          &SharingWebRtcConnection::OnAnswerCreated,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), std::move(sdp))),
      description.release());
}

void SharingWebRtcConnection::OnAnswerCreated(OnOfferReceivedCallback callback,
                                              std::string sdp,
                                              webrtc::RTCError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!error.ok()) {
    LOG(ERROR) << "Failed to set local description: " << error.message() << " ("
               << webrtc::ToString(error.type()) << ")";
    CloseConnection();
    return;
  }

  AddIceCandidates(std::move(ice_candidates_));
  ice_candidates_.clear();
  std::move(callback).Run(sdp);
}

void SharingWebRtcConnection::CreateOffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  peer_connection_->CreateOffer(
      CreateSessionDescriptionObserverWrapper::Create(
          base::BindOnce(&SharingWebRtcConnection::SetLocalOffer,
                         weak_ptr_factory_.GetWeakPtr())),
      options);
}

void SharingWebRtcConnection::SetLocalOffer(
    std::unique_ptr<webrtc::SessionDescriptionInterface> description,
    webrtc::RTCError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string sdp;
  if (!error.ok() || !description || !description->ToString(&sdp)) {
    if (!error.ok()) {
      LOG(ERROR) << "Failed to create local offer: " << error.message() << " ("
                 << webrtc::ToString(error.type()) << ")";
    } else {
      LOG(ERROR) << "Failed to serialize local offer";
    }
    CloseConnection();
    return;
  }

  peer_connection_->SetLocalDescription(
      SetSessionDescriptionObserverWrapper::Create(
          base::BindOnce(&SharingWebRtcConnection::OnOfferCreated,
                         weak_ptr_factory_.GetWeakPtr(), std::move(sdp))),
      description.release());
}

void SharingWebRtcConnection::OnOfferCreated(const std::string& sdp,
                                             webrtc::RTCError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!error.ok()) {
    LOG(ERROR) << "Failed to set local description: " << error.message() << " ("
               << webrtc::ToString(error.type()) << ")";
    CloseConnection();
    return;
  }

  signalling_sender_->SendOffer(
      sdp, base::BindOnce(&SharingWebRtcConnection::OnAnswerReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}

void SharingWebRtcConnection::OnAnswerReceived(const std::string& answer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<webrtc::SessionDescriptionInterface> description(
      webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, answer,
                                       nullptr));
  if (!description) {
    LOG(ERROR) << "Failed to parse received answer";
    CloseConnection();
    return;
  }

  peer_connection_->SetRemoteDescription(
      SetSessionDescriptionObserverWrapper::Create(
          base::BindOnce(&SharingWebRtcConnection::OnRemoteDescriptionSet,
                         weak_ptr_factory_.GetWeakPtr())),
      description.release());
}

void SharingWebRtcConnection::OnRemoteDescriptionSet(webrtc::RTCError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!error.ok()) {
    LOG(ERROR) << "Failed to set remote description: " << error.message()
               << " (" << webrtc::ToString(error.type()) << ")";
    CloseConnection();
    return;
  }

  AddIceCandidates(std::move(ice_candidates_));
  ice_candidates_.clear();
}

void SharingWebRtcConnection::AddIceCandidates(
    std::vector<mojom::IceCandidatePtr> ice_candidates) {
  for (const auto& ice_candidate : ice_candidates) {
    std::unique_ptr<webrtc::IceCandidateInterface> candidate(
        webrtc::CreateIceCandidate(
            ice_candidate->sdp_mid, ice_candidate->sdp_mline_index,
            ice_candidate->candidate, /*error=*/nullptr));
    if (candidate) {
      peer_connection_->AddIceCandidate(
          std::move(candidate), [](webrtc::RTCError error) {
            if (!error.ok()) {
              LOG(ERROR) << "Failed to add IceCandidate: " << error.message()
                         << " (" << webrtc::ToString(error.type()) << ")";
            }
          });
    } else {
      LOG(ERROR) << "Skipping invalid IceCandidate";
    }
  }
}

void SharingWebRtcConnection::CloseConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Call all queued callbacks.
  while (!queued_messages_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(queued_messages_.front().callback),
                                  mojom::SendMessageResult::kError));
    queued_messages_.pop();
  }

  signalling_sender_.reset();
  delegate_.reset();

  // Close DataChannel if necessary.
  if (channel_) {
    switch (channel_->state()) {
      case webrtc::DataChannelInterface::DataState::kClosing:
        // DataChannel is still going through the close procedure and will call
        // OnStateChange when done.
        return;
      case webrtc::DataChannelInterface::DataState::kClosed:
        channel_->UnregisterObserver();
        channel_ = nullptr;
        break;
      default:
        // Start the closing procedure of the DataChannel.
        channel_->Close();
        return;
    }
  }

  // DataChannel must be closed by this point.
  DCHECK(!channel_);

  if (on_disconnect_)
    std::move(on_disconnect_).Run(this);
  // Note: |this| might be destroyed here.
}

void SharingWebRtcConnection::MaybeSendQueuedMessages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (channel_->state() != webrtc::DataChannelInterface::DataState::kOpen)
    return;

  // Send all queued messages. All of them should fit into the DataChannel
  // buffer as we checked the total size before accepting new messages.
  while (!queued_messages_.empty()) {
    if (!channel_->Send(std::move(queued_messages_.front().buffer))) {
      LOG(ERROR) << "Failed to send message";
      CloseConnection();
      return;
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(queued_messages_.front().callback),
                                  mojom::SendMessageResult::kSuccess));
    queued_messages_.pop();
  }
  queued_messages_total_size_ = 0;
}

SharingWebRtcConnection::PendingMessage::PendingMessage(
    webrtc::DataBuffer buffer,
    SendMessageCallback callback)
    : buffer(std::move(buffer)), callback(std::move(callback)) {}

SharingWebRtcConnection::PendingMessage::PendingMessage(
    PendingMessage&& other) = default;

SharingWebRtcConnection::PendingMessage&
SharingWebRtcConnection::PendingMessage::operator=(PendingMessage&& other) =
    default;

SharingWebRtcConnection::PendingMessage::~PendingMessage() = default;

}  // namespace sharing
