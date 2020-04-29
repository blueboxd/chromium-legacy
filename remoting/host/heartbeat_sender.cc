// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include <math.h>

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "remoting/base/constants.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_authenticated_executor.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/base/grpc_support/grpc_util.h"
#include "remoting/base/logging.h"
#include "remoting/base/service_urls.h"
#include "remoting/host/host_details.h"
#include "remoting/host/server_log_entry_host.h"
#include "remoting/proto/remoting/v1/directory_service.grpc.pb.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {

namespace {

constexpr base::TimeDelta kMinimumHeartbeatInterval =
    base::TimeDelta::FromMinutes(3);
constexpr base::TimeDelta kHeartbeatResponseTimeout =
    base::TimeDelta::FromSeconds(30);
constexpr base::TimeDelta kResendDelayOnHostNotFound =
    base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kResendDelayOnUnauthenticated =
    base::TimeDelta::FromSeconds(10);

constexpr int kMaxResendOnHostNotFoundCount =
    12;  // 2 minutes (12 x 10 seconds).
constexpr int kMaxResendOnUnauthenticatedCount =
    6;  // 1 minute (10 x 6 seconds).

const net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms. (10s)
    10000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.5,

    // Maximum amount of time we are willing to delay our request in ms. (10m)
    600000,

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Starts with initial delay.
    false,
};

}  // namespace

class HeartbeatSender::HeartbeatClientImpl final
    : public HeartbeatSender::HeartbeatClient {
 public:
  explicit HeartbeatClientImpl(OAuthTokenGetter* oauth_token_getter);
  ~HeartbeatClientImpl() override;

  void Heartbeat(const apis::v1::HeartbeatRequest& request,
                 HeartbeatResponseCallback callback) override;
  void CancelPendingRequests() override;

 private:
  using DirectoryService = apis::v1::RemotingDirectoryService;
  std::unique_ptr<DirectoryService::Stub> directory_;
  GrpcAuthenticatedExecutor executor_;
  DISALLOW_COPY_AND_ASSIGN(HeartbeatClientImpl);
};

HeartbeatSender::HeartbeatClientImpl::HeartbeatClientImpl(
    OAuthTokenGetter* oauth_token_getter)
    : executor_(oauth_token_getter) {
  directory_ = DirectoryService::NewStub(CreateSslChannelForEndpoint(
      ServiceUrls::GetInstance()->remoting_server_endpoint()));
}

HeartbeatSender::HeartbeatClientImpl::~HeartbeatClientImpl() = default;

void HeartbeatSender::HeartbeatClientImpl::Heartbeat(
    const apis::v1::HeartbeatRequest& request,
    HeartbeatResponseCallback callback) {
  std::string host_offline_reason_or_empty_log =
      request.has_host_offline_reason()
          ? (", host_offline_reason: " + request.host_offline_reason())
          : "";
  HOST_LOG << "Sending outgoing heartbeat. tachyon_id: " << request.tachyon_id()
           << host_offline_reason_or_empty_log;

  auto client_context = std::make_unique<grpc::ClientContext>();
  auto async_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&DirectoryService::Stub::AsyncHeartbeat,
                     base::Unretained(directory_.get())),
      request, std::move(callback));
  SetDeadline(async_request->context(),
              base::Time::Now() + kHeartbeatResponseTimeout);
  executor_.ExecuteRpc(std::move(async_request));
}

void HeartbeatSender::HeartbeatClientImpl::CancelPendingRequests() {
  executor_.CancelPendingRequests();
}

// end of HeartbeatSender::HeartbeatClientImpl

HeartbeatSender::HeartbeatSender(Delegate* delegate,
                                 const std::string& host_id,
                                 SignalStrategy* signal_strategy,
                                 OAuthTokenGetter* oauth_token_getter)
    : delegate_(delegate),
      host_id_(host_id),
      signal_strategy_(signal_strategy),
      client_(std::make_unique<HeartbeatClientImpl>(oauth_token_getter)),
      oauth_token_getter_(oauth_token_getter),
      backoff_(&kBackoffPolicy) {
  DCHECK(delegate_);
  DCHECK(signal_strategy_);

  signal_strategy_->AddListener(this);
  OnSignalStrategyStateChange(signal_strategy_->GetState());
}

HeartbeatSender::~HeartbeatSender() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  signal_strategy_->RemoveListener(this);
}

void HeartbeatSender::SetHostOfflineReason(
    const std::string& host_offline_reason,
    const base::TimeDelta& timeout,
    base::OnceCallback<void(bool success)> ack_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!host_offline_reason_ack_callback_);

  host_offline_reason_ = host_offline_reason;
  host_offline_reason_ack_callback_ = std::move(ack_callback);
  host_offline_reason_timeout_timer_.Start(
      FROM_HERE, timeout, this, &HeartbeatSender::OnHostOfflineReasonTimeout);
  if (signal_strategy_->GetState() == SignalStrategy::State::CONNECTED) {
    SendHeartbeat();
  }
}

void HeartbeatSender::OnSignalStrategyStateChange(SignalStrategy::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state) {
    case SignalStrategy::State::CONNECTED:
      SendHeartbeat();
      break;
    case SignalStrategy::State::DISCONNECTED:
      client_->CancelPendingRequests();
      heartbeat_timer_.AbandonAndStop();
      break;
    default:
      // Do nothing
      break;
  }
}

bool HeartbeatSender::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void HeartbeatSender::OnHostOfflineReasonTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host_offline_reason_ack_callback_);

  std::move(host_offline_reason_ack_callback_).Run(false);
}

void HeartbeatSender::OnHostOfflineReasonAck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!host_offline_reason_ack_callback_) {
    DCHECK(!host_offline_reason_timeout_timer_.IsRunning());
    return;
  }

  DCHECK(host_offline_reason_timeout_timer_.IsRunning());
  host_offline_reason_timeout_timer_.AbandonAndStop();

  std::move(host_offline_reason_ack_callback_).Run(true);
}

void HeartbeatSender::SendHeartbeat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (signal_strategy_->GetState() != SignalStrategy::State::CONNECTED) {
    LOG(WARNING) << "Not sending heartbeat because the signal strategy is not "
                    "connected.";
    return;
  }

  base::TimeDelta last_reported_active_duration =
      signal_strategy_->signaling_tracker()
          .GetLastReportedChannelActiveDuration();
  if (!signal_strategy_->signaling_tracker().IsChannelActive()) {
    LOG(ERROR) << "Signaling channel appears to be inactive but it claims it's "
                  "connected. Last reported active "
               << last_reported_active_duration << " ago.";
    signal_strategy_->Disconnect();
    return;
  } else {
    VLOG(1)
        << "About to send heartbeat. Signaling channel last reported active "
        << last_reported_active_duration << " ago.";
  }

  // Drop previous heartbeat and timer so that it doesn't interfere with the
  // current one.
  client_->CancelPendingRequests();
  heartbeat_timer_.AbandonAndStop();

  client_->Heartbeat(
      CreateHeartbeatRequest(),
      base::BindOnce(&HeartbeatSender::OnResponse, base::Unretained(this)));
}

void HeartbeatSender::OnResponse(const grpc::Status& status,
                                 const apis::v1::HeartbeatResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status.ok()) {
    backoff_.Reset();

    // Notify listener of the first successful heartbeat.
    if (!initial_heartbeat_sent_) {
      delegate_->OnFirstHeartbeatSuccessful();
      initial_heartbeat_sent_ = true;
    }

    // Notify caller of SetHostOfflineReason that we got an ack and don't
    // schedule another heartbeat.
    if (!host_offline_reason_.empty()) {
      OnHostOfflineReasonAck();
      return;
    }

    if (response.has_remote_command()) {
      OnRemoteCommand(response.remote_command());
    }
  } else {
    backoff_.InformOfRequest(false);
  }

  if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
    LOG(ERROR) << "Heartbeat timed out.";
  }

  // If the host was registered immediately before it sends a heartbeat,
  // then server-side latency may prevent the server recognizing the
  // host ID in the heartbeat. So even if all of the first few heartbeats
  // get a "host ID not found" error, that's not a good enough reason to
  // exit.
  if (status.error_code() == grpc::StatusCode::NOT_FOUND &&
      (initial_heartbeat_sent_ ||
       (backoff_.failure_count() > kMaxResendOnHostNotFoundCount))) {
    delegate_->OnHostNotFound();
    return;
  }

  if (status.error_code() == grpc::StatusCode::UNAUTHENTICATED) {
    oauth_token_getter_->InvalidateCache();
    if (backoff_.failure_count() > kMaxResendOnUnauthenticatedCount) {
      delegate_->OnAuthFailed();
      return;
    }
  }

  // Calculate delay before sending the next message.
  base::TimeDelta delay;
  // See CoreErrorDomainTranslator.java for the mapping
  switch (status.error_code()) {
    case grpc::StatusCode::OK:
      delay = base::TimeDelta::FromSeconds(response.set_interval_seconds());
      if (delay < kMinimumHeartbeatInterval) {
        LOG(WARNING) << "Received suspicious set_interval_seconds: " << delay
                     << ". Using minimum interval: "
                     << kMinimumHeartbeatInterval;
        delay = kMinimumHeartbeatInterval;
      }
      break;
    case grpc::StatusCode::NOT_FOUND:
      delay = kResendDelayOnHostNotFound;
      break;
    case grpc::StatusCode::UNAUTHENTICATED:
      delay = kResendDelayOnUnauthenticated;
      break;
    default:
      delay = backoff_.GetTimeUntilRelease();
      LOG(ERROR) << "Heartbeat failed due to unexpected error: "
                 << status.error_code() << ", " << status.error_message()
                 << ". Will retry in " << delay;
      break;
  }

  heartbeat_timer_.Start(FROM_HERE, delay, this,
                         &HeartbeatSender::SendHeartbeat);
}

void HeartbeatSender::OnRemoteCommand(
    apis::v1::HeartbeatResponse::RemoteCommand remote_command) {
  HOST_LOG << "Received remote command: "
           << apis::v1::HeartbeatResponse::RemoteCommand_Name(remote_command)
           << "(" << remote_command << ")";
  switch (remote_command) {
    case apis::v1::HeartbeatResponse::RESTART_HOST:
      delegate_->OnRemoteRestartHost();
      break;
    default:
      LOG(WARNING) << "Unknown remote command: " << remote_command;
      break;
  }
}

apis::v1::HeartbeatRequest HeartbeatSender::CreateHeartbeatRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  apis::v1::HeartbeatRequest heartbeat;
  heartbeat.set_tachyon_id(signal_strategy_->GetLocalAddress().id());
  heartbeat.set_host_id(host_id_);
  if (!host_offline_reason_.empty()) {
    heartbeat.set_host_offline_reason(host_offline_reason_);
  }
  heartbeat.set_host_version(STRINGIZE(VERSION));
  heartbeat.set_host_os_name(GetHostOperatingSystemName());
  heartbeat.set_host_os_version(GetHostOperatingSystemVersion());
  heartbeat.set_host_cpu_type(base::SysInfo::OperatingSystemArchitecture());
  heartbeat.set_is_initial_heartbeat(!initial_heartbeat_sent_);

  return heartbeat;
}

}  // namespace remoting
