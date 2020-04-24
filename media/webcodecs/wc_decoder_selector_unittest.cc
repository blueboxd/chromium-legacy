// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/task_environment.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/status.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder.h"
#include "media/filters/decoder_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "media/webcodecs/wc_decoder_selector.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::StrictMock;

namespace media {

namespace {

enum DecoderCapability {
  kFail,
  kSucceed,
};

const char kNoDecoder[] = "";
const char kDecoder1[] = "Decoder1";
const char kDecoder2[] = "Decoder2";

// Specializations for the AUDIO version of the test.
class AudioDecoderSelectorTestParam {
 public:
  static constexpr DemuxerStream::Type kStreamType = DemuxerStream::AUDIO;

  using DecoderSelector = WebCodecsDecoderSelector<DemuxerStream::AUDIO>;
  using MockDecoder = MockAudioDecoder;
  using Output = AudioBuffer;

  static AudioDecoderConfig CreateConfig() { return TestAudioConfig::Normal(); }

  // Create a config that won't match the return of CreateConfig().
  static AudioDecoderConfig CreateAlternateConfig() {
    return TestAudioConfig::NormalEncrypted();
  }

  // Decoder::Initialize() takes different parameters depending on the type.
  static void ExpectInitialize(MockDecoder* decoder,
                               DecoderCapability capability,
                               AudioDecoderConfig expected_config) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _))
        .WillRepeatedly([capability, expected_config](
                            const AudioDecoderConfig& config, CdmContext*,
                            AudioDecoder::InitCB& init_cb,
                            const AudioDecoder::OutputCB&, const WaitingCB&) {
          EXPECT_TRUE(config.Matches(expected_config));
          std::move(init_cb).Run(capability == kSucceed
                                     ? OkStatus()
                                     : StatusCode::kCodeOnlyForTesting);
        });
  }
};

// Specializations for the VIDEO version of the test.
class VideoDecoderSelectorTestParam {
 public:
  static constexpr DemuxerStream::Type kStreamType = DemuxerStream::VIDEO;

  using DecoderSelector = WebCodecsDecoderSelector<DemuxerStream::VIDEO>;
  using MockDecoder = MockVideoDecoder;
  using Output = VideoFrame;

  static VideoDecoderConfig CreateConfig() { return TestVideoConfig::Normal(); }

  // Create a config that won't match the return of CreateConfig().
  static VideoDecoderConfig CreateAlternateConfig() {
    return TestVideoConfig::LargeEncrypted();
  }

  static void ExpectInitialize(MockDecoder* decoder,
                               DecoderCapability capability,
                               VideoDecoderConfig expected_config) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _, _))
        .WillRepeatedly([capability, expected_config](
                            const VideoDecoderConfig& config, bool low_delay,
                            CdmContext*, VideoDecoder::InitCB& init_cb,
                            const VideoDecoder::OutputCB&, const WaitingCB&) {
          EXPECT_TRUE(config.Matches(expected_config));
          std::move(init_cb).Run(capability == kSucceed
                                     ? OkStatus()
                                     : StatusCode::kCodeOnlyForTesting);
        });
  }
};

// Allocate storage for the member variables.
constexpr DemuxerStream::Type AudioDecoderSelectorTestParam::kStreamType;
constexpr DemuxerStream::Type VideoDecoderSelectorTestParam::kStreamType;

}  // namespace

// Note: The parameter is called TypeParam in the test cases regardless of what
// we call it here. It's been named the same for convenience.
// Note: The test fixtures inherit from this class. Inside the test cases the
// test fixture class is called TestFixture.
template <typename TypeParam>
class WebCodecsDecoderSelectorTest : public ::testing::Test {
 public:
  // Convenience aliases.
  using Self = WebCodecsDecoderSelectorTest<TypeParam>;
  using Decoder = typename TypeParam::DecoderSelector::Decoder;
  using DecoderConfig = typename TypeParam::DecoderSelector::DecoderConfig;
  using MockDecoder = typename TypeParam::MockDecoder;
  using Output = typename TypeParam::Output;

  WebCodecsDecoderSelectorTest() { CreateDecoderSelector(); }

  void OnOutput(scoped_refptr<Output> output) { NOTREACHED(); }

  MOCK_METHOD1_T(OnDecoderSelected, void(std::string));

  void OnDecoderSelectedThunk(std::unique_ptr<Decoder> decoder) {
    // Report only the name of the decoder, since that's what the tests care
    // about. The decoder will be destructed immediately.
    OnDecoderSelected(decoder ? decoder->GetDisplayName() : kNoDecoder);
  }

  void AddMockDecoder(const std::string& decoder_name,
                      DecoderCapability capability) {
    // Actual decoders are created in CreateDecoders(), which may be called
    // multiple times by the DecoderSelector.
    mock_decoders_to_create_.emplace_back(decoder_name, capability);
  }

  std::vector<std::unique_ptr<Decoder>> CreateDecoders() {
    std::vector<std::unique_ptr<Decoder>> decoders;

    for (const auto& info : mock_decoders_to_create_) {
      std::unique_ptr<StrictMock<MockDecoder>> decoder =
          std::make_unique<StrictMock<MockDecoder>>(info.first);
      TypeParam::ExpectInitialize(decoder.get(), info.second,
                                  last_set_decoder_config_);
      decoders.push_back(std::move(decoder));
    }

    return decoders;
  }

  void CreateDecoderSelector() {
    decoder_selector_ =
        std::make_unique<WebCodecsDecoderSelector<TypeParam::kStreamType>>(
            task_environment_.GetMainThreadTaskRunner(),
            base::BindRepeating(&Self::CreateDecoders, base::Unretained(this)),
            base::BindRepeating(&Self::OnOutput, base::Unretained(this)));
  }

  void SelectDecoder(DecoderConfig config = TypeParam::CreateConfig()) {
    last_set_decoder_config_ = config;
    decoder_selector_->SelectDecoder(
        config,
        base::BindOnce(&Self::OnDecoderSelectedThunk, base::Unretained(this)));
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;
  NullMediaLog media_log_;

  DecoderConfig last_set_decoder_config_;

  std::unique_ptr<WebCodecsDecoderSelector<TypeParam::kStreamType>>
      decoder_selector_;

  std::vector<std::pair<std::string, DecoderCapability>>
      mock_decoders_to_create_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCodecsDecoderSelectorTest);
};

using WebCodecsDecoderSelectorTestParams =
    ::testing::Types<AudioDecoderSelectorTestParam,
                     VideoDecoderSelectorTestParam>;
TYPED_TEST_SUITE(WebCodecsDecoderSelectorTest,
                 WebCodecsDecoderSelectorTestParams);

TYPED_TEST(WebCodecsDecoderSelectorTest, NoDecoders) {
  EXPECT_CALL(*this, OnDecoderSelected(kNoDecoder));
  this->SelectDecoder();
}

TYPED_TEST(WebCodecsDecoderSelectorTest, OneDecoder) {
  this->AddMockDecoder(kDecoder1, kSucceed);

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
}

TYPED_TEST(WebCodecsDecoderSelectorTest, TwoDecoders) {
  this->AddMockDecoder(kDecoder1, kFail);
  this->AddMockDecoder(kDecoder2, kSucceed);

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
}

TYPED_TEST(WebCodecsDecoderSelectorTest, TwoDecoders_SelectAgain) {
  this->AddMockDecoder(kDecoder1, kSucceed);
  this->AddMockDecoder(kDecoder2, kSucceed);

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();

  // Selecting again should give (a new instance of) the same decoder.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
}

TYPED_TEST(WebCodecsDecoderSelectorTest, TwoDecoders_NewConfigSelectAgain) {
  this->AddMockDecoder(kDecoder1, kSucceed);
  this->AddMockDecoder(kDecoder2, kSucceed);

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder(TypeParam::CreateConfig());

  // Selecting again should give (a new instance of) the same decoder.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  // Select again with a different config. Expected config verified during
  // CreateDecoders() the SelectDecoder() call.
  this->SelectDecoder(TypeParam::CreateAlternateConfig());
}

}  // namespace media
