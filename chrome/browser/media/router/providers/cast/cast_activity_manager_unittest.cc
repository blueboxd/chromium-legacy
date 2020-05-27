// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/mirroring_activity_record.h"
#include "chrome/browser/media/router/providers/cast/mock_cast_activity_record.h"
#include "chrome/browser/media/router/providers/cast/test_util.h"
#include "chrome/browser/media/router/providers/common/buffered_message_sender.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using base::test::ParseJson;
using testing::_;
using testing::AnyNumber;
using testing::ByRef;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Not;
using testing::Return;
using testing::WithArg;

namespace media_router {

namespace {
constexpr int kChannelId = 42;
constexpr char kOrigin[] = "https://google.com";
constexpr int kTabId = 1;
constexpr char kAppId1[] = "ABCDEFGH";
constexpr char kAppId2[] = "BBBBBBBB";
constexpr char kAppParams[] = R"(
{
  "requiredFeatures" : ["STREAM_TRANSFER"],
  "launchCheckerParams" : {
    "credentialsData" : {
      "credentialsType" : "mobile",
      "credentials" : "99843n2idsguyhga"
    }
  }
}
)";

std::string MakeSourceId(const std::string& app_id = kAppId1,
                         const std::string& app_params = "") {
  return base::StrCat(
      {"cast:", app_id, "?clientId=theClientId&appParams=", app_params});
}

base::Value MakeReceiverStatus(const std::string& app_id,
                               bool update_display_name = false) {
  return ParseJson(R"({
        "applications": [{
          "appId": ")" +
                   app_id +
                   R"(",
          "displayName": "theDisplayName)" +
                   std::string(update_display_name ? "1" : "2") + R"(",
          "namespaces": [
            {"name": "urn:x-cast:com.google.cast.media"},
            {"name": "urn:x-cast:com.google.foo"},
          ],
          "sessionId": "theSessionId",
          "statusText": "theAppStatus",
          "transportId": "theTransportId",
        }],
      })");
}

using MockCastActivityRecordCallback =
    base::RepeatingCallback<void(MockCastActivityRecord*)>;

class MockMirroringActivityRecord : public MirroringActivityRecord {
 public:
  MockMirroringActivityRecord(const MediaRoute& route,
                              const std::string& app_id)
      : MirroringActivityRecord(route,
                                app_id,
                                nullptr,
                                nullptr,
                                0,
                                CastSinkExtraData(),
                                base::DoNothing()) {}

  void set_session_id(const std::string& new_id) {
    if (!session_id_)
      session_id_ = new_id;
    ASSERT_EQ(session_id_, new_id);
  }

  MOCK_METHOD1(CreateMojoBindings, void(mojom::MediaRouter* media_router));
  MOCK_METHOD3(SetOrUpdateSession,
               void(const CastSession& session,
                    const MediaSinkInternal& sink,
                    const std::string& hash_token));
};

}  // namespace

// Test parameters are a boolean indicating whether the client connection should
// be closed by a leave_session message, and the URL used to create the test
// session.
class CastActivityManagerTest : public testing::Test,
                                public ActivityRecordFactoryForTest {
 public:
  CastActivityManagerTest()
      : socket_service_(content::GetUIThreadTaskRunner({})),
        message_handler_(&socket_service_) {
    media_sink_service_.AddOrUpdateSink(sink_);
    socket_.set_id(kChannelId);
  }

  ~CastActivityManagerTest() override = default;

  void SetUp() override {
    CastActivityManager::SetActitivyRecordFactoryForTest(this);

    router_receiver_ = std::make_unique<mojo::Receiver<mojom::MediaRouter>>(
        &mock_router_, router_remote_.BindNewPipeAndPassReceiver());

    session_tracker_.reset(
        new CastSessionTracker(&media_sink_service_, &message_handler_,
                               socket_service_.task_runner()));
    manager_ = std::make_unique<CastActivityManager>(
        &media_sink_service_, session_tracker_.get(), &message_handler_,
        router_remote_.get(), "theHashToken");

    ON_CALL(message_handler_, StopSession)
        .WillByDefault(WithArg<3>([this](auto callback) {
          stop_session_callback_ = std::move(callback);
        }));

    RunUntilIdle();

    // Make sure we get route updates.
    manager_->AddRouteQuery(route_query_);
  }

  void TearDown() override {
    // This is a no-op for many tests, but it serves as a good sanity check in
    // any case.
    RunUntilIdle();

    manager_.reset();
    CastActivityManager::SetActitivyRecordFactoryForTest(nullptr);
  }

  // from ActivityRecordFactoryForTest
  std::unique_ptr<CastActivityRecord> MakeCastActivityRecord(
      const MediaRoute& route,
      const std::string& app_id) override {
    auto activity = std::make_unique<MockCastActivityRecord>(route, app_id);
    auto* activity_ptr = activity.get();
    std::string route_id = route.media_route_id();
    ON_CALL(*activity, SetOrUpdateSession)
        .WillByDefault(WithArg<0>([activity_ptr](const auto& session) {
          activity_ptr->set_session_id(session.session_id());
        }));
    cast_activities_.push_back(activity_ptr);
    activity_record_callback_.Run(activity_ptr);
    return activity;
  }

  // from ActivityRecordFactoryForTest
  std::unique_ptr<MirroringActivityRecord> MakeMirroringActivityRecord(
      const MediaRoute& route,
      const std::string& app_id) override {
    auto activity =
        std::make_unique<MockMirroringActivityRecord>(route, app_id);
    auto* activity_ptr = activity.get();
    ON_CALL(*activity, SetOrUpdateSession)
        .WillByDefault(WithArg<0>([activity_ptr](const auto& session) {
          activity_ptr->set_session_id(session.session_id());
        }));
    mirroring_activity_ = activity.get();
    return activity;
  }

  // Run any pending events and verify expectations associated with them.  This
  // method is sometimes called when there are clearly no pending events simply
  // to check expectations for code executed synchronously.
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&message_handler_);
    testing::Mock::VerifyAndClearExpectations(&mock_router_);
  }

  void ExpectLaunchSessionSuccess(
      const base::Optional<MediaRoute>& route,
      mojom::RoutePresentationConnectionPtr presentation_connections,
      const base::Optional<std::string>&,
      media_router::RouteRequestResult::ResultCode) {
    ASSERT_TRUE(route);
    route_ = std::make_unique<MediaRoute>(*route);
  }

  void ExpectLaunchSessionFailure(
      const base::Optional<MediaRoute>& route,
      mojom::RoutePresentationConnectionPtr presentation_connections,
      const base::Optional<std::string>& error_message,
      media_router::RouteRequestResult::ResultCode result_code) {
    ASSERT_FALSE(route);
    DLOG(ERROR) << error_message.value();
  }

  void CallLaunchSession(const std::string& app_id = kAppId1,
                         const std::string& app_params = "") {
    // MediaRouter is notified of new route.
    ExpectSingleRouteUpdate();

    // A launch session request is sent to the sink.
    std::vector<std::string> supported_app_types = {"WEB"};
    const base::Optional<base::Value> json =
        base::JSONReader().Read(app_params);
    EXPECT_CALL(message_handler_,
                LaunchSession(kChannelId, app_id, kDefaultLaunchTimeout,
                              supported_app_types,
                              testing::Eq(testing::ByRef(json)), _))
        .WillOnce(WithArg<5>([this](auto callback) {
          launch_session_callback_ = std::move(callback);
        }));

    auto source =
        CastMediaSource::FromMediaSourceId(MakeSourceId(app_id, app_params));
    ASSERT_TRUE(source);

    activity_record_callback_ =
        base::BindLambdaForTesting([this](MockCastActivityRecord* activity) {
          // TODO(jrw): Check parameters.
          EXPECT_CALL(*activity, AddClient);
          EXPECT_CALL(*activity, SendMessageToClient).RetiresOnSaturation();
          activity_record_callback_ = base::DoNothing();
        });

    // Callback will be invoked synchronously.
    manager_->LaunchSession(
        *source, sink_, "presentationId", origin_, kTabId,
        /*incognito*/ false,
        base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionSuccess,
                       base::Unretained(this)));

    RunUntilIdle();
  }

  cast_channel::LaunchSessionResponse GetSuccessLaunchResponse() {
    cast_channel::LaunchSessionResponse response;
    response.result = cast_channel::LaunchSessionResponse::Result::kOk;
    response.receiver_status = MakeReceiverStatus(kAppId1);
    return response;
  }

  void LaunchCastAppSession(const std::string& app_id = kAppId1,
                            const std::string& app_params = "") {
    CallLaunchSession(app_id, app_params);

    // 3 things will happen:
    // (1) SDK client receives new_session message.
    // (2) Virtual connection is created.
    // (3) Route list will be updated.

    // TODO(jrw): Check more params.
    EXPECT_CALL(*cast_activities_[0], SendMessageToClient("theClientId", _));
    EXPECT_CALL(*cast_activities_[0], SetOrUpdateSession(_, sink_, _));

    EXPECT_CALL(message_handler_,
                EnsureConnection(kChannelId, "theClientId", "theTransportId"));

    auto response = GetSuccessLaunchResponse();
    session_tracker_->SetSessionForTest(
        route_->media_sink_id(),
        CastSession::From(sink_, *response.receiver_status));
    std::move(launch_session_callback_).Run(std::move(response));
    ExpectSingleRouteUpdate();
    RunUntilIdle();
  }

  void TerminateSession(bool expect_success) {
    EXPECT_CALL(*cast_activities_[0], SendStopSessionMessageToClients);

    // TODO(jrw): Check parameters
    EXPECT_CALL(message_handler_, StopSession(kChannelId, _, _, _));

    if (expect_success) {
      ExpectEmptyRouteUpdate();
    } else {
      ExpectNoRouteUpdate();
    }
    manager_->TerminateSession(route_->media_route_id(),
                               MakeTerminateRouteCallback(expect_success));
    std::move(stop_session_callback_)
        .Run(expect_success ? cast_channel::Result::kOk
                            : cast_channel::Result::kFailed);
  }

  void TerminateNoSession() {
    // Stop session message not sent because session has not launched yet.
    EXPECT_CALL(*cast_activities_[0], SendStopSessionMessageToClients).Times(0);
    EXPECT_CALL(message_handler_, StopSession).Times(0);
    ExpectNoRouteUpdate();
    manager_->TerminateSession(route_->media_route_id(),
                               MakeTerminateRouteCallback(true));
  }

  mojom::MediaRouteProvider::TerminateRouteCallback MakeTerminateRouteCallback(
      bool expect_success) {
    return base::BindLambdaForTesting(
        [expect_success](const base::Optional<std::string>& error_text,
                         RouteRequestResult::ResultCode result_code) {
          if (expect_success) {
            EXPECT_FALSE(error_text.has_value());
            EXPECT_EQ(RouteRequestResult::OK, result_code);
          } else {
            EXPECT_TRUE(error_text.has_value());
            EXPECT_NE(RouteRequestResult::OK, result_code);
          }
        });
  }

  // Expect a call to OnRoutesUpdated() with a single route, which will
  // optionally be saved in the variable pointed to by |route_ptr|.
  void ExpectSingleRouteUpdate() {
    updated_route_ = base::nullopt;
    EXPECT_CALL(mock_router_,
                OnRoutesUpdated(MediaRouteProviderId::CAST, ElementsAre(_),
                                route_query_, IsEmpty()))
        .WillOnce(WithArg<1>(
            [this](const auto& routes) { updated_route_ = routes[0]; }));
  }

  // Expect a call to OnRoutesUpdated() with no routes.
  void ExpectEmptyRouteUpdate() {
    updated_route_ = base::nullopt;
    EXPECT_CALL(mock_router_,
                OnRoutesUpdated(MediaRouteProviderId::CAST, IsEmpty(),
                                route_query_, IsEmpty()))
        .Times(1);
  }

  // Expect that OnRoutesUpdated() will not be called.
  void ExpectNoRouteUpdate() {
    updated_route_ = base::nullopt;
    EXPECT_CALL(mock_router_, OnRoutesUpdated).Times(0);
  }

  std::unique_ptr<CastSession> MakeSession(const std::string& app_id,
                                           bool update_display_name = false) {
    return CastSession::From(sink_,
                             MakeReceiverStatus(app_id, update_display_name));
  }

  // Needed because CastSessionTracker::SetSessionForTest is private.
  void SetSessionForTest(const MediaSink::Id& sink_id,
                         std::unique_ptr<CastSession> session) {
    session_tracker_->SetSessionForTest(sink_id, std::move(session));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  MockMojoMediaRouter mock_router_;
  mojo::Remote<mojom::MediaRouter> router_remote_;
  std::unique_ptr<mojo::Receiver<mojom::MediaRouter>> router_receiver_;
  cast_channel::MockCastSocketService socket_service_;
  cast_channel::MockCastSocket socket_;
  cast_channel::MockCastMessageHandler message_handler_;
  MediaSinkInternal sink_ = CreateCastSink(kChannelId);
  std::unique_ptr<MediaRoute> route_;  // TODO(jrw): Is this needed?
  cast_channel::LaunchSessionCallback launch_session_callback_;
  TestMediaSinkService media_sink_service_;
  MockCastAppDiscoveryService app_discovery_service_;
  std::unique_ptr<CastActivityManager> manager_;
  std::unique_ptr<CastSessionTracker> session_tracker_;
  std::vector<MockCastActivityRecord*> cast_activities_;
  MockMirroringActivityRecord* mirroring_activity_ = nullptr;
  MockCastActivityRecordCallback activity_record_callback_ = base::DoNothing();
  const url::Origin origin_ = url::Origin::Create(GURL(kOrigin));
  const MediaSource::Id route_query_ = "theRouteQuery";
  base::Optional<MediaRoute> updated_route_;
  cast_channel::ResultCallback stop_session_callback_;
};

TEST_F(CastActivityManagerTest, LaunchCastAppSession) {
  LaunchCastAppSession();
  EXPECT_EQ(RouteControllerType::kGeneric, route_->controller_type());
}

TEST_F(CastActivityManagerTest, LaunchCastAppSessionWithAppParams) {
  LaunchCastAppSession(kAppId1, kAppParams);
  EXPECT_EQ(RouteControllerType::kGeneric, route_->controller_type());
}

TEST_F(CastActivityManagerTest, LaunchMirroringSession) {
  CallLaunchSession(kCastStreamingAppId);
  EXPECT_EQ(RouteControllerType::kMirroring, route_->controller_type());
}

TEST_F(CastActivityManagerTest, LaunchSessionFails) {
  // 3 things will happen:
  // (1) Route is removed
  // (2) Issue will be sent.
  // (3) The PresentationConnection associated with the route will be closed
  //     with error.

  CallLaunchSession();

  EXPECT_CALL(
      *cast_activities_[0],
      ClosePresentationConnections(
          blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR));

  cast_channel::LaunchSessionResponse response;
  response.result = cast_channel::LaunchSessionResponse::Result::kError;
  std::move(launch_session_callback_).Run(std::move(response));

  EXPECT_CALL(mock_router_, OnIssue);
  ExpectEmptyRouteUpdate();
  RunUntilIdle();
}

TEST_F(CastActivityManagerTest, LaunchCastAppSessionFailsWithAppParams) {
  auto source =
      CastMediaSource::FromMediaSourceId(MakeSourceId(kAppId1, "invalidjson"));
  ASSERT_TRUE(source);

  // Callback will be invoked synchronously.
  manager_->LaunchSession(
      *source, sink_, "presentationId", origin_, kTabId,
      /*incognito*/ false,
      base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionFailure,
                     base::Unretained(this)));

  RunUntilIdle();
}

TEST_F(CastActivityManagerTest, LaunchSessionTerminatesExistingSessionOnSink) {
  LaunchCastAppSession();

  EXPECT_CALL(*cast_activities_[0], SendStopSessionMessageToClients);

  {
    testing::InSequence dummy;

    // Existing route is terminated before new route is created.
    // MediaRouter is notified of terminated route.
    ExpectEmptyRouteUpdate();

    // After existing route is terminated, new route is created.
    // MediaRouter is notified of new route.
    ExpectSingleRouteUpdate();
  }

  // Launch a new session on the same sink.
  auto source = CastMediaSource::FromMediaSourceId(MakeSourceId(kAppId2));
  // Use LaunchSessionParsed() instead of LaunchSession() here because
  // LaunchSessionParsed() is called asynchronously and will fail the test.
  manager_->LaunchSessionParsed(
      // TODO(jrw): Verify that presentation ID is used correctly.
      *source, sink_, "presentationId2", origin_, kTabId, /*incognito*/
      false,
      base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionSuccess,
                     base::Unretained(this)),
      data_decoder::DataDecoder::ValueOrError());
  std::move(stop_session_callback_).Run(cast_channel::Result::kOk);

  // LaunchSession() should not be called until we notify |mananger_| that the
  // previous session was removed.
  std::vector<std::string> supported_app_types = {"WEB"};
  EXPECT_CALL(message_handler_,
              LaunchSession(kChannelId, "BBBBBBBB", kDefaultLaunchTimeout,
                            supported_app_types,
                            /* base::Optional<base::Value> appParams */
                            testing::Eq(base::nullopt), _));
  manager_->OnSessionRemoved(sink_);
}

TEST_F(CastActivityManagerTest, AddRemoveNonLocalActivity) {
  auto session = MakeSession(kAppId1);
  ExpectSingleRouteUpdate();
  manager_->OnSessionAddedOrUpdated(sink_, *session);
  RunUntilIdle();
  ASSERT_TRUE(updated_route_);
  EXPECT_FALSE(updated_route_->is_local());

  ExpectEmptyRouteUpdate();
  manager_->OnSessionRemoved(sink_);
}

TEST_F(CastActivityManagerTest, UpdateNewlyCreatedSession) {
  LaunchCastAppSession();

  EXPECT_CALL(*cast_activities_[0], SetOrUpdateSession(_, sink_, _));
  auto session = MakeSession(kAppId1);
  ExpectSingleRouteUpdate();
  manager_->OnSessionAddedOrUpdated(sink_, *session);
  RunUntilIdle();
  ASSERT_TRUE(updated_route_);
  EXPECT_TRUE(updated_route_->is_local());
}

// This test is essentially the same as UpdateNewlyCreatedSession, but it uses
// mirroring, which at one point was handled differently enough that this test
// would have failed.
TEST_F(CastActivityManagerTest, UpdateNewlyCreatedMirroringSession) {
  CallLaunchSession(kCastStreamingAppId);
  auto response = GetSuccessLaunchResponse();
  SetSessionForTest(route_->media_sink_id(),
                    CastSession::From(sink_, *response.receiver_status));
  std::move(launch_session_callback_).Run(std::move(response));
  RunUntilIdle();

  ASSERT_TRUE(mirroring_activity_);
  EXPECT_CALL(*mirroring_activity_, SetOrUpdateSession(_, sink_, _));
  auto session = MakeSession(kCastStreamingAppId);
  ExpectSingleRouteUpdate();
  manager_->OnSessionAddedOrUpdated(sink_, *session);
  RunUntilIdle();
  ASSERT_TRUE(updated_route_);
  EXPECT_TRUE(updated_route_->is_local());
}

TEST_F(CastActivityManagerTest, OnSessionAddedOrUpdated) {
  LaunchCastAppSession();
  auto session = MakeSession(kAppId1);
  ExpectSingleRouteUpdate();
  EXPECT_CALL(*cast_activities_[0], SetOrUpdateSession(_, _, "theHashToken"));
  manager_->OnSessionAddedOrUpdated(sink_, *session);
}

// TODO(takumif): Add a test case to terminate a session and launch another.
TEST_F(CastActivityManagerTest, TerminateSession) {
  LaunchCastAppSession();
  TerminateSession(true);
}

TEST_F(CastActivityManagerTest, TerminateSessionFails) {
  LaunchCastAppSession();
  TerminateSession(false);
}

TEST_F(CastActivityManagerTest, TerminateSessionBeforeLaunchResponse) {
  CallLaunchSession();
  TerminateNoSession();
  ExpectEmptyRouteUpdate();
  std::move(launch_session_callback_).Run(GetSuccessLaunchResponse());
}

TEST_F(CastActivityManagerTest, AppMessageFromReceiver) {
  LaunchCastAppSession();

  // Destination ID matches client ID.
  cast::channel::CastMessage message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", base::Value(base::Value::Type::DICTIONARY),
      "sourceId", "theClientId");

  EXPECT_CALL(*cast_activities_[0],
              OnAppMessage(IsCastChannelMessage(message)));
  manager_->OnAppMessage(kChannelId, message);
}

TEST_F(CastActivityManagerTest, OnMediaStatusUpdated) {
  LaunchCastAppSession();

  const char status[] = R"({"foo": "bar"})";
  base::Optional<int> request_id(345);

  EXPECT_CALL(*cast_activities_[0],
              SendMediaStatusToClients(IsJson(status), request_id));
  manager_->OnMediaStatusUpdated(sink_, ParseJson(status), request_id);
}

}  // namespace media_router
