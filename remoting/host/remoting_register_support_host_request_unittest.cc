// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remoting_register_support_host_request.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "remoting/proto/remoting/v1/remote_support_host_service.grpc.pb.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "remoting/signaling/muxing_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;

using RegisterSupportHostResponseCallback =
    base::OnceCallback<void(const grpc::Status&,
                            const apis::v1::RegisterSupportHostResponse&)>;

constexpr char kSupportId[] = "123321456654";
constexpr base::TimeDelta kSupportIdLifetime = base::TimeDelta::FromMinutes(5);
constexpr base::TimeDelta kWaitForAllStrategiesConnectedTimeout =
    base::TimeDelta::FromSecondsD(5.5);
constexpr char kFtlId[] = "fake_user@domain.com/chromoting_ftl_abc123";
constexpr char kJabberId[] = "fake_user@domain.com/chromotingABC123";

void ValidateRegisterHost(const apis::v1::RegisterSupportHostRequest& request,
                          bool expects_ftl_id,
                          bool expects_jabber_id) {
  ASSERT_TRUE(request.has_host_version());
  ASSERT_TRUE(request.has_host_os_name());
  ASSERT_TRUE(request.has_host_os_version());

  if (expects_ftl_id) {
    ASSERT_EQ(kFtlId, request.tachyon_id());
  }
  if (expects_jabber_id) {
    ASSERT_EQ(kJabberId, request.jabber_id());
  }

  auto key_pair = RsaKeyPair::FromString(kTestRsaKeyPair);
  EXPECT_EQ(key_pair->GetPublicKey(), request.public_key());
}

void RespondOk(RegisterSupportHostResponseCallback callback) {
  apis::v1::RegisterSupportHostResponse response;
  response.set_support_id(kSupportId);
  response.set_support_id_lifetime_seconds(kSupportIdLifetime.InSeconds());
  std::move(callback).Run(grpc::Status::OK, response);
}

decltype(auto) DoValidateRegisterHostAndRespondOk(bool expects_ftl_id,
                                                  bool expects_jabber_id) {
  return [=](const apis::v1::RegisterSupportHostRequest& request,
             RegisterSupportHostResponseCallback callback) {
    ValidateRegisterHost(request, expects_ftl_id, expects_jabber_id);
    RespondOk(std::move(callback));
  };
}

}  // namespace

class RemotingRegisterSupportHostTest : public testing::Test {
 public:
  RemotingRegisterSupportHostTest() {
    register_host_request_ =
        std::make_unique<RemotingRegisterSupportHostRequest>(
            std::make_unique<FakeOAuthTokenGetter>(
                OAuthTokenGetter::Status::SUCCESS, "fake_email",
                "fake_access_token"));

    auto register_host_client =
        std::make_unique<MockRegisterSupportHostClient>();
    register_host_client_ = register_host_client.get();
    register_host_request_->register_host_client_ =
        std::move(register_host_client);

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

    key_pair_ = RsaKeyPair::FromString(kTestRsaKeyPair);
  }

  ~RemotingRegisterSupportHostTest() override {
    register_host_request_.reset();
    muxing_signal_strategy_.reset();
    scoped_task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  class MockRegisterSupportHostClient final
      : public RemotingRegisterSupportHostRequest::RegisterSupportHostClient {
   public:
    MOCK_METHOD2(RegisterSupportHost,
                 void(const apis::v1::RegisterSupportHostRequest&,
                      RegisterSupportHostResponseCallback));
    MOCK_METHOD0(CancelPendingRequests, void());
  };

  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME_AND_NOW};

  std::unique_ptr<RemotingRegisterSupportHostRequest> register_host_request_;
  MockRegisterSupportHostClient* register_host_client_ = nullptr;

  std::unique_ptr<MuxingSignalStrategy> muxing_signal_strategy_;
  FakeSignalStrategy* ftl_signal_strategy_;
  FakeSignalStrategy* xmpp_signal_strategy_;

  scoped_refptr<RsaKeyPair> key_pair_;
};

TEST_F(RemotingRegisterSupportHostTest, RegisterOnlyFtl) {
  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce(
          DoValidateRegisterHostAndRespondOk(/* expects_ftl_id */ true,
                                             /* expects_jabber_id */ false));
  EXPECT_CALL(*register_host_client_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(kSupportId, kSupportIdLifetime, protocol::ErrorCode::OK))
      .Times(1);

  register_host_request_->StartRequest(muxing_signal_strategy_.get(), key_pair_,
                                       register_callback.Get());
  ftl_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
}

TEST_F(RemotingRegisterSupportHostTest, RegisterOnlyXmpp) {
  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce(
          DoValidateRegisterHostAndRespondOk(/* expects_ftl_id */ false,
                                             /* expects_jabber_id */ true));
  EXPECT_CALL(*register_host_client_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(kSupportId, kSupportIdLifetime, protocol::ErrorCode::OK))
      .Times(1);

  register_host_request_->StartRequest(muxing_signal_strategy_.get(), key_pair_,
                                       register_callback.Get());
  xmpp_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
}

TEST_F(RemotingRegisterSupportHostTest, RegisterBoth) {
  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce(
          DoValidateRegisterHostAndRespondOk(/* expects_ftl_id */ true,
                                             /* expects_jabber_id */ true));
  EXPECT_CALL(*register_host_client_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(kSupportId, kSupportIdLifetime, protocol::ErrorCode::OK))
      .Times(1);

  register_host_request_->StartRequest(muxing_signal_strategy_.get(), key_pair_,
                                       register_callback.Get());
  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(RemotingRegisterSupportHostTest,
       XmppConnectedAfterTimeout_OnlyRegistersFtl) {
  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce(
          DoValidateRegisterHostAndRespondOk(/* expects_ftl_id */ true,
                                             /* expects_jabber_id */ false));
  EXPECT_CALL(*register_host_client_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(kSupportId, kSupportIdLifetime, protocol::ErrorCode::OK))
      .Times(1);

  register_host_request_->StartRequest(muxing_signal_strategy_.get(), key_pair_,
                                       register_callback.Get());
  ftl_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
  xmpp_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(RemotingRegisterSupportHostTest, FailedWithDeadlineExceeded) {
  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce([](const apis::v1::RegisterSupportHostRequest& request,
                   RegisterSupportHostResponseCallback callback) {
        ValidateRegisterHost(request, true, true);
        std::move(callback).Run(
            grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED,
                         "deadline exceeded"),
            {});
      });
  EXPECT_CALL(*register_host_client_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(_, _, protocol::ErrorCode::SIGNALING_TIMEOUT))
      .Times(1);

  register_host_request_->StartRequest(muxing_signal_strategy_.get(), key_pair_,
                                       register_callback.Get());
  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(RemotingRegisterSupportHostTest,
       SignalingDisconnectedBeforeRegistrationSucceeds) {
  RegisterSupportHostResponseCallback register_support_host_callback;
  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce([&](const apis::v1::RegisterSupportHostRequest& request,
                    RegisterSupportHostResponseCallback callback) {
        ValidateRegisterHost(request, true, true);
        register_support_host_callback = std::move(callback);
      });
  EXPECT_CALL(*register_host_client_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(_, _, protocol::ErrorCode::SIGNALING_ERROR))
      .Times(1);

  register_host_request_->StartRequest(muxing_signal_strategy_.get(), key_pair_,
                                       register_callback.Get());
  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  ftl_signal_strategy_->Disconnect();
  xmpp_signal_strategy_->Disconnect();
  scoped_task_environment_.FastForwardUntilNoTasksRemain();

  RespondOk(std::move(register_support_host_callback));
}

}  // namespace remoting
