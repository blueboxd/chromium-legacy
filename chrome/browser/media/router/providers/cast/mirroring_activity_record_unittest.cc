// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mirroring_activity_record.h"

#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/media/router/providers/cast/activity_record_test_base.h"
#include "chrome/browser/media/router/providers/cast/test_util.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "components/cast_channel/cast_test_util.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::test::IsJson;
using testing::_;
using testing::WithArg;

namespace media_router {
namespace {

constexpr int kTabId = 123;
constexpr char kDescription[] = "";
constexpr char kDesktopMediaId[] = "theDesktopMediaId";
constexpr char kPresentationId[] = "thePresentationId";

class MockMirroringServiceHost : public mirroring::mojom::MirroringServiceHost {
 public:
  MOCK_METHOD4(
      Start,
      void(mirroring::mojom::SessionParametersPtr params,
           mojo::PendingRemote<mirroring::mojom::SessionObserver> observer,
           mojo::PendingRemote<mirroring::mojom::CastMessageChannel>
               outbound_channel,
           mojo::PendingReceiver<mirroring::mojom::CastMessageChannel>
               inbound_channel));
};

class MockCastMessageChannel : public mirroring::mojom::CastMessageChannel {
 public:
  MOCK_METHOD1(Send, void(mirroring::mojom::CastMessagePtr message));
};

}  // namespace

class MirroringActivityRecordTest
    : public ActivityRecordTestBase,
      public testing::WithParamInterface<const char* /*namespace*/> {
 protected:
  void SetUp() override {
    ActivityRecordTestBase::SetUp();

    auto make_mirroring_service =
        [this](mojo::PendingReceiver<mirroring::mojom::MirroringServiceHost>
                   receiver) {
          ASSERT_FALSE(mirroring_service_);
          auto mirroring_service = std::make_unique<MockMirroringServiceHost>();
          mirroring_service_ = mirroring_service.get();
          mojo::MakeSelfOwnedReceiver(std::move(mirroring_service),
                                      std::move(receiver));
        };
    ON_CALL(media_router_, GetMirroringServiceHostForDesktop)
        .WillByDefault(WithArg<2>(make_mirroring_service));
    ON_CALL(media_router_, GetMirroringServiceHostForTab)
        .WillByDefault(WithArg<1>(make_mirroring_service));
    ON_CALL(media_router_, GetMirroringServiceHostForOffscreenTab)
        .WillByDefault(WithArg<2>(make_mirroring_service));
  }

  void MakeRecord() { MakeRecord(MediaSource::ForTab(kTabId), kTabId); }

  void MakeRecord(const MediaSource& source, int tab_id = -1) {
    CastSinkExtraData cast_data;
    cast_data.cast_channel_id = kChannelId;
    cast_data.capabilities = cast_channel::AUDIO_OUT | cast_channel::VIDEO_OUT;
    MediaRoute route(kRouteId, source, kSinkId, kDescription, route_is_local_,
                     true);
    route.set_presentation_id(kPresentationId);
    record_ = std::make_unique<MirroringActivityRecord>(
        route, kAppId, &message_handler_, &session_tracker_, kTabId, cast_data,
        on_stop_.Get());

    if (route_is_local_) {
      record_->CreateMojoBindings(&media_router_);

      EXPECT_CALL(*mirroring_service_, Start)
          .WillOnce(WithArg<3>(
              [this](mojo::PendingReceiver<mirroring::mojom::CastMessageChannel>
                         inbound_channel) {
                ASSERT_FALSE(channel_to_service_);
                auto channel = std::make_unique<MockCastMessageChannel>();
                channel_to_service_ = channel.get();
                mojo::MakeSelfOwnedReceiver(std::move(channel),
                                            std::move(inbound_channel));
              }));
    }

    record_->SetOrUpdateSession(*session_, sink_, kHashToken);
    RunUntilIdle();
  }

  bool route_is_local_ = true;
  MockCastMessageChannel* channel_to_service_ = nullptr;
  MockMirroringServiceHost* mirroring_service_ = nullptr;
  MockMojoMediaRouter media_router_;
  base::MockCallback<MirroringActivityRecord::OnStopCallback> on_stop_;
  std::unique_ptr<MirroringActivityRecord> record_;
};

INSTANTIATE_TEST_CASE_P(Namespaces,
                        MirroringActivityRecordTest,
                        testing::Values(mirroring::mojom::kWebRtcNamespace,
                                        mirroring::mojom::kRemotingNamespace));

TEST_F(MirroringActivityRecordTest, CreateMojoBindingsForDesktop) {
  EXPECT_CALL(media_router_,
              GetMirroringServiceHostForDesktop(_, kDesktopMediaId, _));
  MediaSource source = MediaSource::ForDesktop(kDesktopMediaId);
  ASSERT_TRUE(source.IsDesktopMirroringSource());
  MakeRecord(source);
}

TEST_F(MirroringActivityRecordTest, CreateMojoBindingsForTab) {
  EXPECT_CALL(media_router_, GetMirroringServiceHostForTab(kTabId, _));
  MediaSource source = MediaSource::ForTab(kTabId);
  ASSERT_TRUE(source.IsTabMirroringSource());
  MakeRecord(source, kTabId);
}

TEST_F(MirroringActivityRecordTest, CreateMojoBindingsForTabWithCastAppUrl) {
  GURL url(kMirroringAppUri);
  EXPECT_CALL(media_router_, GetMirroringServiceHostForTab(kTabId, _));
  MediaSource source = MediaSource::ForPresentationUrl(url);
  ASSERT_TRUE(source.IsCastPresentationUrl());
  MakeRecord(source, kTabId);
}

TEST_F(MirroringActivityRecordTest, CreateMojoBindingsForOffscreenTab) {
  static constexpr char kUrl[] = "http://wikipedia.org";
  GURL url(kUrl);
  EXPECT_CALL(media_router_,
              GetMirroringServiceHostForOffscreenTab(url, kPresentationId, _));
  MediaSource source = MediaSource::ForPresentationUrl(url);
  ASSERT_FALSE(source.IsCastPresentationUrl());
  MakeRecord(source);
}

TEST_F(MirroringActivityRecordTest, OnError) {
  MakeRecord();
  EXPECT_CALL(on_stop_, Run());
  record_->OnError(mirroring::mojom::SessionError::CAST_TRANSPORT_ERROR);
  RunUntilIdle();
}

TEST_F(MirroringActivityRecordTest, DidStop) {
  MakeRecord();
  EXPECT_CALL(on_stop_, Run());
  record_->DidStop();
  RunUntilIdle();
}

TEST_F(MirroringActivityRecordTest, SendWebRtc) {
  MakeRecord();
  static constexpr char kPayload[] = R"({"foo": "bar"})";
  EXPECT_CALL(message_handler_, SendCastMessage(kChannelId, _))
      .WillOnce(
          WithArg<1>([this](const cast::channel::CastMessage& cast_message) {
            EXPECT_EQ(message_handler_.sender_id(), cast_message.source_id());
            EXPECT_EQ("theTransportId", cast_message.destination_id());
            EXPECT_EQ(mirroring::mojom::kWebRtcNamespace,
                      cast_message.namespace_());
            EXPECT_TRUE(cast_message.has_payload_utf8());
            EXPECT_THAT(cast_message.payload_utf8(), IsJson(kPayload));
            EXPECT_FALSE(cast_message.has_payload_binary());
            return cast_channel::Result::kOk;
          }));

  record_->Send(mirroring::mojom::CastMessage::New("the_namespace", kPayload));
  RunUntilIdle();
}

TEST_F(MirroringActivityRecordTest, SendRemoting) {
  MakeRecord();
  static constexpr char kPayload[] = R"({"type": "RPC"})";
  EXPECT_CALL(message_handler_, SendCastMessage(kChannelId, _))
      .WillOnce(WithArg<1>([](const cast::channel::CastMessage& cast_message) {
        EXPECT_EQ(mirroring::mojom::kRemotingNamespace,
                  cast_message.namespace_());
        return cast_channel::Result::kOk;
      }));

  record_->Send(mirroring::mojom::CastMessage::New("the_namespace", kPayload));
  RunUntilIdle();
}

TEST_F(MirroringActivityRecordTest, OnAppMessageWrongNamespace) {
  MakeRecord();
  EXPECT_CALL(*channel_to_service_, Send).Times(0);
  cast::channel::CastMessage message;
  message.set_namespace_("wrong_namespace");
  record_->OnAppMessage(message);
}

TEST_P(MirroringActivityRecordTest, OnAppMessageWrongNonlocal) {
  route_is_local_ = false;
  MakeRecord();
  ASSERT_FALSE(channel_to_service_);
  cast::channel::CastMessage message;
  message.set_namespace_(GetParam());
  record_->OnAppMessage(message);
}

TEST_P(MirroringActivityRecordTest, OnAppMessage) {
  MakeRecord();

  static constexpr char kPayload[] = R"({"foo": "bar"})";

  EXPECT_CALL(*channel_to_service_, Send)
      .WillOnce([](mirroring::mojom::CastMessagePtr message) {
        EXPECT_EQ(GetParam(), message->message_namespace);
        EXPECT_EQ(kPayload, message->json_format_data);
      });

  cast::channel::CastMessage message;
  message.set_namespace_(GetParam());
  message.set_protocol_version(
      cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_payload_utf8(kPayload);
  record_->OnAppMessage(message);
}

TEST_F(MirroringActivityRecordTest, OnInternalMessageNonlocal) {
  route_is_local_ = false;
  MakeRecord();
  ASSERT_FALSE(channel_to_service_);
  record_->OnInternalMessage(cast_channel::InternalMessage(
      cast_channel::CastMessageType::kPing, "the_namespace", base::Value()));
}

TEST_F(MirroringActivityRecordTest, OnInternalMessage) {
  MakeRecord();

  static constexpr char kPayload[] = R"({"foo": "bar"})";
  static constexpr char kNamespace[] = "the_namespace";

  EXPECT_CALL(*channel_to_service_, Send)
      .WillOnce([](mirroring::mojom::CastMessagePtr message) {
        EXPECT_EQ(kNamespace, message->message_namespace);
        EXPECT_THAT(message->json_format_data, IsJson(kPayload));
      });

  record_->OnInternalMessage(cast_channel::InternalMessage(
      cast_channel::CastMessageType::kPing, kNamespace,
      base::test::ParseJson(kPayload)));
}

}  // namespace media_router
