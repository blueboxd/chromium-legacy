// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS Controller.

#include "content/browser/speech/tts_controller_impl.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/browser/speech/tts_utterance_impl.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/browser/visibility.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"

#if defined(OS_CHROMEOS)
#include "content/public/browser/tts_controller_delegate.h"
#endif

namespace content {

// Platform Tts implementation that does nothing.
class MockTtsPlatformImpl : public TtsPlatform {
 public:
  MockTtsPlatformImpl() = default;
  virtual ~MockTtsPlatformImpl() = default;

  void set_voices(const std::vector<VoiceData>& voices) { voices_ = voices; }

  void set_run_speak_callback(bool value) { run_speak_callback_ = value; }
  void set_is_speaking(bool value) { is_speaking_ = value; }

  // TtsPlatform:
  bool PlatformImplAvailable() override { return true; }
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override {
    if (run_speak_callback_)
      std::move(on_speak_finished).Run(true);
  }
  bool IsSpeaking() override { return is_speaking_; }
  bool StopSpeaking() override { return true; }
  void Pause() override {}
  void Resume() override {}
  void GetVoices(std::vector<VoiceData>* out_voices) override {
    *out_voices = voices_;
  }
  bool LoadBuiltInTtsEngine(BrowserContext* browser_context) override {
    return false;
  }
  void WillSpeakUtteranceWithVoice(TtsUtterance* utterance,
                                   const VoiceData& voice_data) override {}
  void SetError(const std::string& error) override {}
  std::string GetError() override { return std::string(); }
  void ClearError() override {}

 private:
  std::vector<VoiceData> voices_;
  bool run_speak_callback_ = true;
  bool is_speaking_ = false;
};

#if defined(OS_CHROMEOS)
class MockTtsControllerDelegate : public TtsControllerDelegate {
 public:
  MockTtsControllerDelegate() = default;
  ~MockTtsControllerDelegate() override = default;

  void SetPreferredVoiceIds(const PreferredVoiceIds& ids) { ids_ = ids; }

  BrowserContext* GetLastBrowserContext() {
    BrowserContext* result = last_browser_context_;
    last_browser_context_ = nullptr;
    return result;
  }

  // TtsControllerDelegate:
  std::unique_ptr<PreferredVoiceIds> GetPreferredVoiceIdsForUtterance(
      TtsUtterance* utterance) override {
    last_browser_context_ = utterance->GetBrowserContext();
    auto ids = std::make_unique<PreferredVoiceIds>(ids_);
    return ids;
  }

  void UpdateUtteranceDefaultsFromPrefs(content::TtsUtterance* utterance,
                                        double* rate,
                                        double* pitch,
                                        double* volume) override {}

 private:
  BrowserContext* last_browser_context_ = nullptr;
  PreferredVoiceIds ids_;
};
#endif

// Subclass of TtsController with a public ctor and dtor.
class TtsControllerForTesting : public TtsControllerImpl {
 public:
  TtsControllerForTesting() {}
  ~TtsControllerForTesting() override {}
};

TEST(TtsControllerTest, TestTtsControllerShutdown) {
  MockTtsPlatformImpl platform_impl;
  std::unique_ptr<TtsControllerForTesting> controller =
      std::make_unique<TtsControllerForTesting>();
#if defined(OS_CHROMEOS)
  MockTtsControllerDelegate delegate;
  controller->delegate_ = &delegate;
#endif

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
  controller.reset();
}

#if defined(OS_CHROMEOS)
TEST(TtsControllerTest, TestBrowserContextRemoved) {
  // Create a controller, mock other stuff, and create a test
  // browser context.
  TtsControllerImpl* controller = TtsControllerImpl::GetInstance();
  MockTtsPlatformImpl platform_impl;
  MockTtsControllerDelegate delegate;
  controller->delegate_ = &delegate;
  controller->SetTtsPlatform(&platform_impl);
  content::BrowserTaskEnvironment task_environment;
  auto browser_context = std::make_unique<TestBrowserContext>();

  std::vector<VoiceData> voices;
  VoiceData voice_data;
  voice_data.engine_id = "x";
  voice_data.events.insert(TTS_EVENT_END);
  voices.push_back(voice_data);
  platform_impl.set_voices(voices);

  // Speak an utterances associated with this test browser context.
  std::unique_ptr<TtsUtterance> utterance1 =
      TtsUtterance::Create(browser_context.get());
  utterance1->SetEngineId("x");
  utterance1->SetCanEnqueue(true);
  utterance1->SetSrcId(1);
  controller->SpeakOrEnqueue(std::move(utterance1));

  // Assert that the delegate was called and it got our browser context.
  ASSERT_EQ(browser_context.get(), delegate.GetLastBrowserContext());

  // Now queue up a second utterance to be spoken, also associated with
  // this browser context.
  std::unique_ptr<TtsUtterance> utterance2 =
      TtsUtterance::Create(browser_context.get());
  utterance2->SetEngineId("x");
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
#else
TEST(TtsControllerTest, TestTtsControllerUtteranceDefaults) {
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
#endif

TEST(TtsControllerTest, TestGetMatchingVoice) {
  std::unique_ptr<TtsControllerForTesting> controller =
      std::make_unique<TtsControllerForTesting>();
#if defined(OS_CHROMEOS)
  MockTtsControllerDelegate delegate;
  controller->delegate_ = &delegate;
#endif

  TestContentBrowserClient::GetInstance()->set_application_locale("en");

  {
    // Calling GetMatchingVoice with no voices returns -1.
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create(nullptr));
    std::vector<VoiceData> voices;
    EXPECT_EQ(-1, controller->GetMatchingVoice(utterance.get(), voices));
  }

  {
    // Calling GetMatchingVoice with any voices returns the first one
    // even if there are no criteria that match.
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create(nullptr));
    std::vector<VoiceData> voices(2);
    EXPECT_EQ(0, controller->GetMatchingVoice(utterance.get(), voices));
  }

  {
    // If nothing else matches, the English voice is returned.
    // (In tests the language will always be English.)
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create(nullptr));
    std::vector<VoiceData> voices;
    VoiceData fr_voice;
    fr_voice.lang = "fr";
    voices.push_back(fr_voice);
    VoiceData en_voice;
    en_voice.lang = "en";
    voices.push_back(en_voice);
    VoiceData de_voice;
    de_voice.lang = "de";
    voices.push_back(de_voice);
    EXPECT_EQ(1, controller->GetMatchingVoice(utterance.get(), voices));
  }

  {
    // Check precedence of various matching criteria.
    std::vector<VoiceData> voices;
    VoiceData voice0;
    voices.push_back(voice0);
    VoiceData voice1;
    voice1.events.insert(TTS_EVENT_WORD);
    voices.push_back(voice1);
    VoiceData voice2;
    voice2.lang = "de-DE";
    voices.push_back(voice2);
    VoiceData voice3;
    voice3.lang = "fr-CA";
    voices.push_back(voice3);
    VoiceData voice4;
    voice4.name = "Voice4";
    voices.push_back(voice4);
    VoiceData voice5;
    voice5.engine_id = "id5";
    voices.push_back(voice5);
    VoiceData voice6;
    voice6.engine_id = "id7";
    voice6.name = "Voice6";
    voice6.lang = "es-es";
    voices.push_back(voice6);
    VoiceData voice7;
    voice7.engine_id = "id7";
    voice7.name = "Voice7";
    voice7.lang = "es-mx";
    voices.push_back(voice7);
    VoiceData voice8;
    voice8.engine_id = "";
    voice8.name = "Android";
    voice8.lang = "";
    voice8.native = true;
    voices.push_back(voice8);

    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create(nullptr));
    EXPECT_EQ(0, controller->GetMatchingVoice(utterance.get(), voices));

    std::set<TtsEventType> types;
    types.insert(TTS_EVENT_WORD);
    utterance->SetRequiredEventTypes(types);
    EXPECT_EQ(1, controller->GetMatchingVoice(utterance.get(), voices));

    utterance->SetLang("de-DE");
    EXPECT_EQ(2, controller->GetMatchingVoice(utterance.get(), voices));

    utterance->SetLang("fr-FR");
    EXPECT_EQ(3, controller->GetMatchingVoice(utterance.get(), voices));

    utterance->SetVoiceName("Voice4");
    EXPECT_EQ(4, controller->GetMatchingVoice(utterance.get(), voices));

    utterance->SetVoiceName("");
    utterance->SetEngineId("id5");
    EXPECT_EQ(5, controller->GetMatchingVoice(utterance.get(), voices));

#if defined(OS_CHROMEOS)
    TtsControllerDelegate::PreferredVoiceIds preferred_voice_ids;
    preferred_voice_ids.locale_voice_id.emplace("Voice7", "id7");
    preferred_voice_ids.any_locale_voice_id.emplace("Android", "");
    delegate.SetPreferredVoiceIds(preferred_voice_ids);

    // Voice6 is matched when the utterance locale exactly matches its locale.
    utterance->SetEngineId("");
    utterance->SetLang("es-es");
    EXPECT_EQ(6, controller->GetMatchingVoice(utterance.get(), voices));

    // The 7th voice is the default for "es", even though the utterance is
    // "es-ar". |voice6| is not matched because it is not the default.
    utterance->SetEngineId("");
    utterance->SetLang("es-ar");
    EXPECT_EQ(7, controller->GetMatchingVoice(utterance.get(), voices));

    // The 8th voice is like the built-in "Android" voice, it has no lang
    // and no extension ID. Make sure it can still be matched.
    preferred_voice_ids.locale_voice_id.reset();
    delegate.SetPreferredVoiceIds(preferred_voice_ids);
    utterance->SetVoiceName("Android");
    utterance->SetEngineId("");
    utterance->SetLang("");
    EXPECT_EQ(8, controller->GetMatchingVoice(utterance.get(), voices));

    delegate.SetPreferredVoiceIds({});
#endif
  }

  {
    // Check voices against system language.
    std::vector<VoiceData> voices;
    VoiceData voice0;
    voice0.engine_id = "id0";
    voice0.name = "voice0";
    voice0.lang = "en-GB";
    voices.push_back(voice0);
    VoiceData voice1;
    voice1.engine_id = "id1";
    voice1.name = "voice1";
    voice1.lang = "en-US";
    voices.push_back(voice1);
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create(nullptr));

    // voice1 is matched against the exact default system language.
    TestContentBrowserClient::GetInstance()->set_application_locale("en-US");
    utterance->SetLang("");
    EXPECT_EQ(1, controller->GetMatchingVoice(utterance.get(), voices));

#if defined(OS_CHROMEOS)
    // voice0 is matched against the system language which has no region piece.
    TestContentBrowserClient::GetInstance()->set_application_locale("en");
    EXPECT_EQ(0, controller->GetMatchingVoice(utterance.get(), voices));

    TtsControllerDelegate::PreferredVoiceIds preferred_voice_ids2;
    preferred_voice_ids2.locale_voice_id.emplace("voice0", "id0");
    delegate.SetPreferredVoiceIds(preferred_voice_ids2);
    // voice0 is matched against the pref over the system language.
    TestContentBrowserClient::GetInstance()->set_application_locale("en-US");
    EXPECT_EQ(0, controller->GetMatchingVoice(utterance.get(), voices));
#endif
  }
}

class TtsControllerTestHelper {
 public:
  TtsControllerTestHelper() {
    controller_.SetTtsPlatform(&platform_impl_);
    // This ensures utterances don't immediately complete.
    platform_impl_.set_run_speak_callback(false);
    platform_impl_.set_is_speaking(true);
  }

  std::unique_ptr<TestWebContents> CreateWebContents() {
    return std::unique_ptr<TestWebContents>(
        TestWebContents::Create(&browser_context_, nullptr));
  }

  std::unique_ptr<TtsUtteranceImpl> CreateUtterance(WebContents* web_contents) {
    return std::make_unique<TtsUtteranceImpl>(&browser_context_, web_contents);
  }

  MockTtsPlatformImpl* platform_impl() { return &platform_impl_; }

  TtsControllerForTesting* controller() { return &controller_; }

  TtsUtterance* TtsControllerCurrentUtterance() {
    return controller_.current_utterance_.get();
  }

  bool IsUtteranceListEmpty() { return controller_.utterance_list_.empty(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  RenderViewHostTestEnabler rvh_enabler_;
  TestBrowserContext browser_context_;
  MockTtsPlatformImpl platform_impl_;
  TtsControllerForTesting controller_;
};

TEST(TtsControllerTest, StopsWhenWebContentsDestroyed) {
  TtsControllerTestHelper helper;
  std::unique_ptr<WebContents> web_contents = helper.CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance =
      helper.CreateUtterance(web_contents.get());

  helper.controller()->SpeakOrEnqueue(std::move(utterance));
  EXPECT_TRUE(helper.controller()->IsSpeaking());
  EXPECT_TRUE(helper.TtsControllerCurrentUtterance());

  web_contents.reset();
  // Destroying the WebContents should reset
  // |TtsController::current_utterance_|.
  EXPECT_FALSE(helper.TtsControllerCurrentUtterance());
}

TEST(TtsControllerTest, StartsQueuedUtteranceWhenWebContentsDestroyed) {
  TtsControllerTestHelper helper;
  std::unique_ptr<WebContents> web_contents1 = helper.CreateWebContents();
  std::unique_ptr<WebContents> web_contents2 = helper.CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance1 =
      helper.CreateUtterance(web_contents1.get());
  void* raw_utterance1 = utterance1.get();
  std::unique_ptr<TtsUtteranceImpl> utterance2 =
      helper.CreateUtterance(web_contents2.get());
  utterance2->SetCanEnqueue(true);
  void* raw_utterance2 = utterance2.get();

  helper.controller()->SpeakOrEnqueue(std::move(utterance1));
  EXPECT_TRUE(helper.controller()->IsSpeaking());
  EXPECT_TRUE(helper.TtsControllerCurrentUtterance());
  helper.controller()->SpeakOrEnqueue(std::move(utterance2));
  EXPECT_EQ(raw_utterance1, helper.TtsControllerCurrentUtterance());

  web_contents1.reset();
  // Destroying |web_contents1| should delete |utterance1| and start
  // |utterance2|.
  EXPECT_TRUE(helper.TtsControllerCurrentUtterance());
  EXPECT_EQ(raw_utterance2, helper.TtsControllerCurrentUtterance());
}

TEST(TtsControllerTest, StartsQueuedUtteranceWhenWebContentsDestroyed2) {
  TtsControllerTestHelper helper;
  std::unique_ptr<WebContents> web_contents1 = helper.CreateWebContents();
  std::unique_ptr<WebContents> web_contents2 = helper.CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance1 =
      helper.CreateUtterance(web_contents1.get());
  void* raw_utterance1 = utterance1.get();
  std::unique_ptr<TtsUtteranceImpl> utterance2 =
      helper.CreateUtterance(web_contents1.get());
  std::unique_ptr<TtsUtteranceImpl> utterance3 =
      helper.CreateUtterance(web_contents2.get());
  void* raw_utterance3 = utterance3.get();
  utterance2->SetCanEnqueue(true);
  utterance3->SetCanEnqueue(true);

  helper.controller()->SpeakOrEnqueue(std::move(utterance1));
  helper.controller()->SpeakOrEnqueue(std::move(utterance2));
  helper.controller()->SpeakOrEnqueue(std::move(utterance3));
  EXPECT_TRUE(helper.controller()->IsSpeaking());
  EXPECT_EQ(raw_utterance1, helper.TtsControllerCurrentUtterance());

  web_contents1.reset();
  // Deleting |web_contents1| should delete |utterance1| and |utterance2| as
  // they are both from |web_contents1|. |raw_utterance3| should be made the
  // current as it's from a different WebContents.
  EXPECT_EQ(raw_utterance3, helper.TtsControllerCurrentUtterance());
  EXPECT_TRUE(helper.IsUtteranceListEmpty());

  web_contents2.reset();
  // Deleting |web_contents2| should delete |utterance3| as it's from a
  // different WebContents.
  EXPECT_EQ(nullptr, helper.TtsControllerCurrentUtterance());
}

TEST(TtsControllerTest, StartsUtteranceWhenWebContentsHidden) {
  TtsControllerTestHelper helper;
  std::unique_ptr<TestWebContents> web_contents = helper.CreateWebContents();
  web_contents->SetVisibilityAndNotifyObservers(Visibility::HIDDEN);
  std::unique_ptr<TtsUtteranceImpl> utterance =
      helper.CreateUtterance(web_contents.get());
  helper.controller()->SpeakOrEnqueue(std::move(utterance));
  EXPECT_TRUE(helper.controller()->IsSpeaking());
}

TEST(TtsControllerTest,
     DoesNotStartUtteranceWhenWebContentsHiddenAndStopSpeakingWhenHiddenSet) {
  TtsControllerTestHelper helper;
  std::unique_ptr<TestWebContents> web_contents = helper.CreateWebContents();
  web_contents->SetVisibilityAndNotifyObservers(Visibility::HIDDEN);
  std::unique_ptr<TtsUtteranceImpl> utterance =
      helper.CreateUtterance(web_contents.get());
  helper.controller()->SetStopSpeakingWhenHidden(true);
  helper.controller()->SpeakOrEnqueue(std::move(utterance));
  EXPECT_EQ(nullptr, helper.TtsControllerCurrentUtterance());
  EXPECT_TRUE(helper.IsUtteranceListEmpty());
}

TEST(TtsControllerTest, SkipsQueuedUtteranceFromHiddenWebContents) {
  TtsControllerTestHelper helper;
  helper.controller()->SetStopSpeakingWhenHidden(true);
  std::unique_ptr<WebContents> web_contents1 = helper.CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents2 = helper.CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance1 =
      helper.CreateUtterance(web_contents1.get());
  const int utterance1_id = utterance1->GetId();
  std::unique_ptr<TtsUtteranceImpl> utterance2 =
      helper.CreateUtterance(web_contents2.get());
  utterance2->SetCanEnqueue(true);

  helper.controller()->SpeakOrEnqueue(std::move(utterance1));
  EXPECT_TRUE(helper.TtsControllerCurrentUtterance());
  EXPECT_TRUE(helper.IsUtteranceListEmpty());

  // Speak |utterance2|, which should get queued.
  helper.controller()->SpeakOrEnqueue(std::move(utterance2));
  EXPECT_FALSE(helper.IsUtteranceListEmpty());

  // Make the second WebContents hidden, this shouldn't change anything in
  // TtsController.
  web_contents2->SetVisibilityAndNotifyObservers(Visibility::HIDDEN);
  EXPECT_FALSE(helper.IsUtteranceListEmpty());

  // Finish |utterance1|, which should skip |utterance2| because |web_contents2|
  // is hidden.
  helper.controller()->OnTtsEvent(utterance1_id, TTS_EVENT_END, 0, 0, {});
  EXPECT_EQ(nullptr, helper.TtsControllerCurrentUtterance());
  EXPECT_TRUE(helper.IsUtteranceListEmpty());
}

}  // namespace content
