// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS Controller.

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/browser/speech/tts_controller_impl.h"
#include "content/public/browser/tts_controller_delegate.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"

namespace content {

class TtsControllerTest : public testing::Test {};

// Platform Tts implementation that does nothing.
class MockTtsPlatformImpl : public TtsPlatform {
 public:
  MockTtsPlatformImpl() {}
  virtual ~MockTtsPlatformImpl() {}
  bool PlatformImplAvailable() override { return true; }
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override {
    std::move(on_speak_finished).Run(true);
  }
  bool IsSpeaking() override { return false; }
  bool StopSpeaking() override { return true; }
  void Pause() override {}
  void Resume() override {}
  void GetVoices(std::vector<VoiceData>* out_voices) override {}
  bool LoadBuiltInTtsEngine(BrowserContext* browser_context) override {
    return false;
  }
  void WillSpeakUtteranceWithVoice(TtsUtterance* utterance,
                                   const VoiceData& voice_data) override {}
  void SetError(const std::string& error) override {}
  std::string GetError() override { return std::string(); }
  void ClearError() override {}
};

class MockTtsControllerDelegate : public TtsControllerDelegate {
 public:
  MockTtsControllerDelegate() {}
  ~MockTtsControllerDelegate() override {}

  BrowserContext* GetLastBrowserContext() {
    BrowserContext* result = last_browser_context_;
    last_browser_context_ = nullptr;
    return result;
  }

  int GetMatchingVoice(content::TtsUtterance* utterance,
                       std::vector<content::VoiceData>& voices) override {
    last_browser_context_ = utterance->GetBrowserContext();
    return -1;
  }

  void UpdateUtteranceDefaultsFromPrefs(content::TtsUtterance* utterance,
                                        double* rate,
                                        double* pitch,
                                        double* volume) override {}

  void SetTtsEngineDelegate(content::TtsEngineDelegate* delegate) override {}

  content::TtsEngineDelegate* GetTtsEngineDelegate() override {
    return nullptr;
  }

 private:
  BrowserContext* last_browser_context_ = nullptr;
};

// Subclass of TtsController with a public ctor and dtor.
class TtsControllerForTesting : public TtsControllerImpl {
 public:
  TtsControllerForTesting() {}
  ~TtsControllerForTesting() override {}
};

TEST_F(TtsControllerTest, TestTtsControllerShutdown) {
  MockTtsPlatformImpl platform_impl;
  TtsControllerForTesting* controller = new TtsControllerForTesting();
  MockTtsControllerDelegate* delegate = new MockTtsControllerDelegate();
  controller->delegate_ = delegate;

  controller->SetTtsPlatform(&platform_impl);

  std::unique_ptr<TtsUtterance> utterance1 = TtsUtterance::Create(nullptr);
  utterance1->SetCanEnqueue(true);
  utterance1->SetSrcId(1);
  controller->SpeakOrEnqueue(std::move(utterance1));

  std::unique_ptr<TtsUtterance> utterance2 = TtsUtterance::Create(nullptr);
  utterance2->SetCanEnqueue(true);
  utterance2->SetSrcId(2);
  controller->SpeakOrEnqueue(std::move(utterance2));

  // Make sure that deleting the controller when there are pending
  // utterances doesn't cause a crash.
  delete controller;

  // Clean up.
  delete delegate;
}

TEST_F(TtsControllerTest, TestBrowserContextRemoved) {
  // Create a controller, mock other stuff, and create a test
  // browser context.
  TtsControllerImpl* controller = TtsControllerImpl::GetInstance();
  MockTtsPlatformImpl platform_impl;
  MockTtsControllerDelegate delegate;
  controller->delegate_ = &delegate;
  controller->SetTtsPlatform(&platform_impl);
  content::BrowserTaskEnvironment task_environment;
  auto browser_context = std::make_unique<TestBrowserContext>();

  // Speak an utterances associated with this test browser context.
  std::unique_ptr<TtsUtterance> utterance1 =
      TtsUtterance::Create(browser_context.get());
  utterance1->SetCanEnqueue(true);
  utterance1->SetSrcId(1);
  controller->SpeakOrEnqueue(std::move(utterance1));

  // Assert that the delegate was called and it got our browser context.
  ASSERT_EQ(browser_context.get(), delegate.GetLastBrowserContext());

  // Now queue up a second utterance to be spoken, also associated with
  // this browser context.
  std::unique_ptr<TtsUtterance> utterance2 =
      TtsUtterance::Create(browser_context.get());
  utterance2->SetCanEnqueue(true);
  utterance2->SetSrcId(2);
  controller->SpeakOrEnqueue(std::move(utterance2));

  // Destroy the browser context before the utterance is spoken.
  browser_context.reset();

  // Now speak the next utterance, and ensure that we don't get the
  // destroyed browser context.
  controller->FinishCurrentUtterance();
  controller->SpeakNextUtterance();
  ASSERT_EQ(nullptr, delegate.GetLastBrowserContext());
}

#if !defined(OS_CHROMEOS)
TEST_F(TtsControllerTest, TestTtsControllerUtteranceDefaults) {
  std::unique_ptr<TtsControllerForTesting> controller =
      std::make_unique<TtsControllerForTesting>();

  std::unique_ptr<TtsUtterance> utterance1 =
      content::TtsUtterance::Create(nullptr);
  // Initialized to default (unset constant) values.
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDoublePrefNotSet,
            utterance1->GetContinuousParameters().rate);
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDoublePrefNotSet,
            utterance1->GetContinuousParameters().pitch);
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDoublePrefNotSet,
            utterance1->GetContinuousParameters().volume);

  controller->UpdateUtteranceDefaults(utterance1.get());
  // Updated to global defaults.
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDefaultRate,
            utterance1->GetContinuousParameters().rate);
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDefaultPitch,
            utterance1->GetContinuousParameters().pitch);
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDefaultVolume,
            utterance1->GetContinuousParameters().volume);
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace content
