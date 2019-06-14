// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/proto/remoting/v1/directory_service.grpc.pb.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "remoting/signaling/log_to_server.h"
#include "remoting/signaling/muxing_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;
using testing::AtMost;
using testing::InSequence;
using testing::Return;

using HeartbeatResponseCallback =
    base::OnceCallback<void(const grpc::Status&,
                            const apis::v1::HeartbeatResponse&)>;

constexpr char kOAuthAccessToken[] = "fake_access_token";
constexpr char kHostId[] = "fake_host_id";
constexpr char kUserEmail[] = "fake_user@domain.com";

constexpr char kFtlId[] = "fake_user@domain.com/chromoting_ftl_abc123";
constexpr char kJabberId[] = "fake_user@domain.com/chromotingABC123";

constexpr int32_t kGoodIntervalSeconds = 300;

constexpr base::TimeDelta kWaitForAllStrategiesConnectedTimeout =
    base::TimeDelta::FromSecondsD(5.5);
constexpr base::TimeDelta kOfflineReasonTimeout =
    base::TimeDelta::FromSeconds(123);
constexpr base::TimeDelta kTestHeartbeatDelay =
    base::TimeDelta::FromSeconds(350);

void ValidateHeartbeat(const apis::v1::HeartbeatRequest& request,
                       bool expects_ftl_id,
                       bool expects_jabber_id,
                       const std::string& expected_host_offline_reason = {}) {
  ASSERT_TRUE(request.has_host_version());
  if (expected_host_offline_reason.empty()) {
    ASSERT_FALSE(request.has_host_offline_reason());
  } else {
    ASSERT_EQ(expected_host_offline_reason, request.host_offline_reason());
  }
  ASSERT_EQ(kHostId, request.host_id());

  std::string signaling_id;
  if (expects_ftl_id) {
    ASSERT_EQ(kFtlId, request.tachyon_id());
    signaling_id = kFtlId;
  }
  if (expects_jabber_id) {
    ASSERT_EQ(kJabberId, request.jabber_id());
    if (signaling_id.empty()) {
      signaling_id = kJabberId;
    }
  }
}

decltype(auto) DoValidateHeartbeatAndRespondOk(
    bool expects_ftl_id,
    bool expects_jabber_id,
    const std::string& expected_host_offline_reason = {}) {
  return [=](const apis::v1::HeartbeatRequest& request,
             HeartbeatResponseCallback callback) {
    ValidateHeartbeat(request, expects_ftl_id, expects_jabber_id,
                      expected_host_offline_reason);
    apis::v1::HeartbeatResponse response;
    response.set_set_interval_seconds(kGoodIntervalSeconds);
    std::move(callback).Run(grpc::Status::OK, response);
  };
}

}  // namespace

class HeartbeatSenderTest : public testing::Test, public LogToServer {
 public:
  HeartbeatSenderTest() {
    auto ftl_signal_strategy =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kFtlId));
    auto xmpp_signal_strategy =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kJabberId));
    ftl_signal_strategy_ = ftl_signal_strategy.get();
    xmpp_signal_strategy_ = xmpp_signal_strategy.get();

    // Start in disconnected state.
    ftl_signal_strategy_->Disconnect();
    xmpp_signal_strategy_->Disconnect();

    muxing_signal_strategy_ = std::make_unique<MuxingSignalStrategy>(
        std::move(ftl_signal_strategy), std::move(xmpp_signal_strategy));

    heartbeat_sender_ = std::make_unique<HeartbeatSender>(
        mock_heartbeat_successful_callback_.Get(),
        mock_unknown_host_id_error_callback_.Get(),
        mock_unauthenticated_error_callback_.Get(), kHostId,
        muxing_signal_strategy_.get(), &oauth_token_getter_, this);
    auto heartbeat_client = std::make_unique<MockHeartbeatClient>();
    mock_client_ = heartbeat_client.get();
    heartbeat_sender_->client_ = std::move(heartbeat_client);
  }

  ~HeartbeatSenderTest() override {
    heartbeat_sender_.reset();
    muxing_signal_strategy_.reset();
    scoped_task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  class MockHeartbeatClient : public HeartbeatSender::HeartbeatClient {
   public:
    MOCK_METHOD2(Heartbeat,
                 void(const apis::v1::HeartbeatRequest&,
                      HeartbeatResponseCallback));

    void CancelPendingRequests() override {
      // We just don't care about this method being called.
    }
  };

  HeartbeatSender* heartbeat_sender() { return heartbeat_sender_.get(); }

  const net::BackoffEntry& GetBackoff() const {
    return heartbeat_sender_->backoff_;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
      base::test::ScopedTaskEnvironment::NowSource::MAIN_THREAD_MOCK_TIME};
  MockHeartbeatClient* mock_client_;

  FakeSignalStrategy* ftl_signal_strategy_;
  FakeSignalStrategy* xmpp_signal_strategy_;

  base::MockCallback<base::OnceClosure> mock_heartbeat_successful_callback_;
  base::MockCallback<base::OnceClosure> mock_unknown_host_id_error_callback_;
  base::MockCallback<base::OnceClosure> mock_unauthenticated_error_callback_;

  std::vector<ServerLogEntry> received_log_entries_;

 private:
  // LogToServer interface.
  void Log(const ServerLogEntry& entry) override {
    received_log_entries_.push_back(entry);
  }

  ServerLogEntry::Mode mode() const override { return ServerLogEntry::ME2ME; }

  std::unique_ptr<MuxingSignalStrategy> muxing_signal_strategy_;

  // |heartbeat_sender_| must be deleted before |muxing_signal_strategy_|.
  std::unique_ptr<HeartbeatSender> heartbeat_sender_;

  FakeOAuthTokenGetter oauth_token_getter_{OAuthTokenGetter::Status::SUCCESS,
                                           kUserEmail, kOAuthAccessToken};
};

TEST_F(HeartbeatSenderTest, SendHeartbeat_OnlyXmpp) {
  EXPECT_CALL(*mock_client_, Heartbeat(_, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ false,
                                                /* xmpp */ true));

  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).Times(1);

  xmpp_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
}

TEST_F(HeartbeatSenderTest, SendHeartbeat_OnlyFtl) {
  EXPECT_CALL(*mock_client_, Heartbeat(_, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ false));

  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).Times(1);

  ftl_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
}

TEST_F(HeartbeatSenderTest, SendHeartbeat_XmppAndFtl) {
  EXPECT_CALL(*mock_client_, Heartbeat(_, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ true));

  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).Times(1);

  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();
}

TEST_F(HeartbeatSenderTest, SendHeartbeat_SendFtlThenBoth) {
  base::RunLoop run_loop;

  EXPECT_CALL(*mock_client_, Heartbeat(_, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ false))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ true));

  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).Times(1);

  ftl_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
  xmpp_signal_strategy_->Connect();
}

TEST_F(HeartbeatSenderTest, SignalingReconnect_NewHeartbeats) {
  base::RunLoop run_loop;

  EXPECT_CALL(*mock_client_, Heartbeat(_, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ true))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ false,
                                                /* xmpp */ true))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ true))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ false,
                                                /* xmpp */ true))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ true));

  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).Times(1);

  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();
  ftl_signal_strategy_->Disconnect();
  ftl_signal_strategy_->Connect();
  ftl_signal_strategy_->Disconnect();
  xmpp_signal_strategy_->Disconnect();
  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();
}

TEST_F(HeartbeatSenderTest, SetHostOfflineReason) {
  base::MockCallback<base::OnceCallback<void(bool success)>> mock_ack_callback;
  EXPECT_CALL(mock_ack_callback, Run(_)).Times(0);

  heartbeat_sender()->SetHostOfflineReason("test_error", kOfflineReasonTimeout,
                                           mock_ack_callback.Get());

  testing::Mock::VerifyAndClearExpectations(&mock_ack_callback);

  EXPECT_CALL(*mock_client_, Heartbeat(_, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ true, "test_error"));

  // Callback should run once, when we get response to offline-reason.
  EXPECT_CALL(mock_ack_callback, Run(_)).Times(1);
  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).Times(1);

  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();
}

TEST_F(HeartbeatSenderTest, HostOsInfoOnFirstHeartbeat) {
  EXPECT_CALL(*mock_client_, Heartbeat(_, _))
      .WillOnce([](const apis::v1::HeartbeatRequest& request,
                   HeartbeatResponseCallback callback) {
        ValidateHeartbeat(request, /* ftl */ true, /* xmpp */ true);
        // First heartbeat has host OS info.
        EXPECT_TRUE(request.has_host_os_name());
        EXPECT_TRUE(request.has_host_os_version());
        apis::v1::HeartbeatResponse response;
        response.set_set_interval_seconds(kGoodIntervalSeconds);
        std::move(callback).Run(grpc::Status::OK, response);
      });
  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).Times(1);
  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();

  EXPECT_CALL(*mock_client_, Heartbeat(_, _))
      .WillOnce([&](const apis::v1::HeartbeatRequest& request,
                    HeartbeatResponseCallback callback) {
        ValidateHeartbeat(request, /* ftl */ true, /* xmpp */ true);
        // Subsequent heartbeat has no host OS info.
        EXPECT_FALSE(request.has_host_os_name());
        EXPECT_FALSE(request.has_host_os_version());
        apis::v1::HeartbeatResponse response;
        response.set_set_interval_seconds(kGoodIntervalSeconds);
        std::move(callback).Run(grpc::Status::OK, response);
      });
  scoped_task_environment_.FastForwardBy(kTestHeartbeatDelay);
}

TEST_F(HeartbeatSenderTest, UnknownHostId) {
  EXPECT_CALL(*mock_client_, Heartbeat(_, _))
      .WillRepeatedly([](const apis::v1::HeartbeatRequest& request,
                         HeartbeatResponseCallback callback) {
        ValidateHeartbeat(request, /* ftl */ true, /* xmpp */ true);
        std::move(callback).Run(
            grpc::Status(grpc::StatusCode::NOT_FOUND, "not found"), {});
      });

  EXPECT_CALL(mock_unknown_host_id_error_callback_, Run()).Times(1);

  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(HeartbeatSenderTest, SendHeartbeatLogEntryOnHeartbeat) {
  EXPECT_CALL(*mock_client_, Heartbeat(_, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ true));

  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).Times(1);

  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();

  ASSERT_EQ(1u, received_log_entries_.size());
}

TEST_F(HeartbeatSenderTest, FailedToHeartbeat_Backoff) {
  {
    InSequence sequence;

    EXPECT_CALL(*mock_client_, Heartbeat(_, _))
        .Times(2)
        .WillRepeatedly([&](const apis::v1::HeartbeatRequest& request,
                            HeartbeatResponseCallback callback) {
          ValidateHeartbeat(request, /* ftl */ true, /* xmpp */ true);
          std::move(callback).Run(
              grpc::Status(grpc::StatusCode::UNAVAILABLE, "unavailable"), {});
        });

    EXPECT_CALL(*mock_client_, Heartbeat(_, _))
        .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                  /* xmpp */ true));
  }

  ASSERT_EQ(0, GetBackoff().failure_count());
  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();
  ASSERT_EQ(1, GetBackoff().failure_count());
  scoped_task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_EQ(2, GetBackoff().failure_count());
  scoped_task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_EQ(0, GetBackoff().failure_count());
}

TEST_F(HeartbeatSenderTest, Unauthenticated) {
  int heartbeat_count = 0;
  EXPECT_CALL(*mock_client_, Heartbeat(_, _))
      .WillRepeatedly([&](const apis::v1::HeartbeatRequest& request,
                          HeartbeatResponseCallback callback) {
        ValidateHeartbeat(request, /* ftl */ true, /* xmpp */ true);
        heartbeat_count++;
        std::move(callback).Run(
            grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "unauthenticated"),
            {});
      });

  EXPECT_CALL(mock_unauthenticated_error_callback_, Run()).Times(1);

  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardUntilNoTasksRemain();

  // Should retry heartbeating at least once.
  ASSERT_LT(1, heartbeat_count);
}

}  // namespace remoting
