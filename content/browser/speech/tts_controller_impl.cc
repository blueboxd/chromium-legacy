// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_controller_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/system_connector.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/web_speech_synthesis_constants.h"

namespace content {

// A value to be used to indicate that there is no char index available.
const int kInvalidCharIndex = -1;

// A value to be used to indicate that there is no length available.
const int kInvalidLength = -1;

//
// VoiceData
//

VoiceData::VoiceData() : remote(false), native(false) {}

VoiceData::VoiceData(const VoiceData& other) = default;

VoiceData::~VoiceData() {}

//
// TtsController
//

TtsController* TtsController::GetInstance() {
  return TtsControllerImpl::GetInstance();
}

// IMPORTANT!
// These values are written to logs.  Do not renumber or delete
// existing items; add new entries to the end of the list.
enum class UMATextToSpeechEvent {
  START = 0,
  END = 1,
  WORD = 2,
  SENTENCE = 3,
  MARKER = 4,
  INTERRUPTED = 5,
  CANCELLED = 6,
  SPEECH_ERROR = 7,
  PAUSE = 8,
  RESUME = 9,

  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  COUNT
};

//
// TtsControllerImpl
//

// static
TtsControllerImpl* TtsControllerImpl::GetInstance() {
  return base::Singleton<TtsControllerImpl>::get();
}

TtsControllerImpl::TtsControllerImpl()
    : delegate_(nullptr),
      current_utterance_(nullptr),
      paused_(false),
      tts_platform_(nullptr) {}

TtsControllerImpl::~TtsControllerImpl() {
  if (current_utterance_) {
    current_utterance_->Finish();
    delete current_utterance_;
  }

  // Clear any queued utterances too.
  ClearUtteranceQueue(false);  // Don't sent events.
}

void TtsControllerImpl::SpeakOrEnqueue(TtsUtterance* utterance) {
  // If we're paused and we get an utterance that can't be queued,
  // flush the queue but stay in the paused state.
  if (paused_ && !utterance->GetCanEnqueue()) {
    utterance_queue_.push(utterance);
    Stop();
    paused_ = true;
    return;
  }

  if (paused_ || (IsSpeaking() && utterance->GetCanEnqueue())) {
    utterance_queue_.push(utterance);
  } else {
    Stop();
    SpeakNow(utterance);
  }
}

void TtsControllerImpl::Stop() {
  Stop(GURL());
}

void TtsControllerImpl::Stop(const GURL& source_url) {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Stop"));

  paused_ = false;

  if (!source_url.is_empty() && current_utterance_ &&
      current_utterance_->GetSrcUrl().GetOrigin() != source_url.GetOrigin())
    return;

  if (current_utterance_ && !current_utterance_->GetEngineId().empty()) {
    if (GetTtsControllerDelegate()->GetTtsEngineDelegate())
      GetTtsControllerDelegate()->GetTtsEngineDelegate()->Stop(
          current_utterance_);
  } else {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->StopSpeaking();
  }

  if (current_utterance_)
    current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                                   kInvalidLength, std::string());
  FinishCurrentUtterance();
  ClearUtteranceQueue(true);  // Send events.
}

void TtsControllerImpl::Pause() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Pause"));

  paused_ = true;
  if (current_utterance_ && !current_utterance_->GetEngineId().empty()) {
    if (GetTtsControllerDelegate()->GetTtsEngineDelegate())
      GetTtsControllerDelegate()->GetTtsEngineDelegate()->Pause(
          current_utterance_);
  } else if (current_utterance_) {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->Pause();
  }
}

void TtsControllerImpl::Resume() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Resume"));

  paused_ = false;
  if (current_utterance_ && !current_utterance_->GetEngineId().empty()) {
    if (GetTtsControllerDelegate()->GetTtsEngineDelegate())
      GetTtsControllerDelegate()->GetTtsEngineDelegate()->Resume(
          current_utterance_);
  } else if (current_utterance_) {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->Resume();
  } else {
    SpeakNextUtterance();
  }
}

void TtsControllerImpl::OnTtsEvent(int utterance_id,
                                   TtsEventType event_type,
                                   int char_index,
                                   int length,
                                   const std::string& error_message) {
  // We may sometimes receive completion callbacks "late", after we've
  // already finished the utterance (for example because another utterance
  // interrupted or we got a call to Stop). This is normal and we can
  // safely just ignore these events.
  if (!current_utterance_ || utterance_id != current_utterance_->GetId()) {
    return;
  }

  UMATextToSpeechEvent metric;
  switch (event_type) {
    case TTS_EVENT_START:
      metric = UMATextToSpeechEvent::START;
      break;
    case TTS_EVENT_END:
      metric = UMATextToSpeechEvent::END;
      break;
    case TTS_EVENT_WORD:
      metric = UMATextToSpeechEvent::WORD;
      break;
    case TTS_EVENT_SENTENCE:
      metric = UMATextToSpeechEvent::SENTENCE;
      break;
    case TTS_EVENT_MARKER:
      metric = UMATextToSpeechEvent::MARKER;
      break;
    case TTS_EVENT_INTERRUPTED:
      metric = UMATextToSpeechEvent::INTERRUPTED;
      break;
    case TTS_EVENT_CANCELLED:
      metric = UMATextToSpeechEvent::CANCELLED;
      break;
    case TTS_EVENT_ERROR:
      metric = UMATextToSpeechEvent::SPEECH_ERROR;
      break;
    case TTS_EVENT_PAUSE:
      metric = UMATextToSpeechEvent::PAUSE;
      break;
    case TTS_EVENT_RESUME:
      metric = UMATextToSpeechEvent::RESUME;
      break;
    default:
      NOTREACHED();
      return;
  }
  UMA_HISTOGRAM_ENUMERATION("TextToSpeech.Event", metric,
                            UMATextToSpeechEvent::COUNT);

  current_utterance_->OnTtsEvent(event_type, char_index, length, error_message);
  if (current_utterance_->IsFinished()) {
    FinishCurrentUtterance();
    SpeakNextUtterance();
  }
}

void TtsControllerImpl::GetVoices(BrowserContext* browser_context,
                                  std::vector<VoiceData>* out_voices) {
  TtsPlatform* tts_platform = GetTtsPlatform();
  if (tts_platform) {
    // Ensure we have all built-in voices loaded. This is a no-op if already
    // loaded.
    tts_platform->LoadBuiltInTtsEngine(browser_context);
    if (tts_platform->PlatformImplAvailable())
      tts_platform->GetVoices(out_voices);
  }

  if (browser_context && GetTtsControllerDelegate()->GetTtsEngineDelegate())
    GetTtsControllerDelegate()->GetTtsEngineDelegate()->GetVoices(
        browser_context, out_voices);
}

bool TtsControllerImpl::IsSpeaking() {
  return current_utterance_ != nullptr || GetTtsPlatform()->IsSpeaking();
}

void TtsControllerImpl::VoicesChanged() {
  // Existence of platform tts indicates explicit requests to tts. Since
  // |VoicesChanged| can occur implicitly, only send if needed.
  for (auto& delegate : voices_changed_delegates_)
    delegate.OnVoicesChanged();
}

void TtsControllerImpl::AddVoicesChangedDelegate(
    VoicesChangedDelegate* delegate) {
  voices_changed_delegates_.AddObserver(delegate);
}

void TtsControllerImpl::RemoveVoicesChangedDelegate(
    VoicesChangedDelegate* delegate) {
  voices_changed_delegates_.RemoveObserver(delegate);
}

void TtsControllerImpl::RemoveUtteranceEventDelegate(
    UtteranceEventDelegate* delegate) {
  // First clear any pending utterances with this delegate.
  base::queue<TtsUtterance*> old_queue = utterance_queue_;
  utterance_queue_ = base::queue<TtsUtterance*>();
  while (!old_queue.empty()) {
    TtsUtterance* utterance = old_queue.front();
    old_queue.pop();
    if (utterance->GetEventDelegate() != delegate)
      utterance_queue_.push(utterance);
    else
      delete utterance;
  }

  if (current_utterance_ &&
      current_utterance_->GetEventDelegate() == delegate) {
    current_utterance_->SetEventDelegate(nullptr);
    if (!current_utterance_->GetEngineId().empty()) {
      if (GetTtsControllerDelegate()->GetTtsEngineDelegate())
        GetTtsControllerDelegate()->GetTtsEngineDelegate()->Stop(
            current_utterance_);
    } else {
      GetTtsPlatform()->ClearError();
      GetTtsPlatform()->StopSpeaking();
    }

    FinishCurrentUtterance();
    if (!paused_)
      SpeakNextUtterance();
  }
}

void TtsControllerImpl::SetTtsEngineDelegate(TtsEngineDelegate* delegate) {
  if (!GetTtsControllerDelegate())
    return;

  GetTtsControllerDelegate()->SetTtsEngineDelegate(delegate);
}

TtsEngineDelegate* TtsControllerImpl::GetTtsEngineDelegate() {
  if (!GetTtsControllerDelegate())
    return nullptr;

  return GetTtsControllerDelegate()->GetTtsEngineDelegate();
}

void TtsControllerImpl::SetTtsPlatform(TtsPlatform* tts_platform) {
  tts_platform_ = tts_platform;
}

int TtsControllerImpl::QueueSize() {
  return static_cast<int>(utterance_queue_.size());
}

TtsPlatform* TtsControllerImpl::GetTtsPlatform() {
  if (!tts_platform_)
    tts_platform_ = TtsPlatform::GetInstance();
  return tts_platform_;
}

void TtsControllerImpl::SpeakNow(TtsUtterance* utterance) {
  if (!GetTtsControllerDelegate())
    return;

  // Get all available voices and try to find a matching voice.
  std::vector<VoiceData> voices;
  GetVoices(utterance->GetBrowserContext(), &voices);

  // Get the best matching voice. If nothing matches, just set "native"
  // to true because that might trigger deferred loading of native voices.
  // TODO(katie): Move most of the GetMatchingVoice logic into content/ and
  // use the TTS controller delegate to get chrome-specific info as needed.
  int index = GetTtsControllerDelegate()->GetMatchingVoice(utterance, voices);
  VoiceData voice;
  if (index >= 0)
    voice = voices[index];
  else
    voice.native = true;

  UpdateUtteranceDefaults(utterance);

  GetTtsPlatform()->WillSpeakUtteranceWithVoice(utterance, voice);

  base::RecordAction(base::UserMetricsAction("TextToSpeech.Speak"));
  UMA_HISTOGRAM_COUNTS_100000("TextToSpeech.Utterance.TextLength",
                              utterance->GetText().size());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.FromExtensionAPI",
                        !utterance->GetSrcUrl().is_empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasVoiceName",
                        !utterance->GetVoiceName().empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasLang",
                        !utterance->GetLang().empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasRate",
                        utterance->GetContinuousParameters().rate != 1.0);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasPitch",
                        utterance->GetContinuousParameters().pitch != 1.0);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasVolume",
                        utterance->GetContinuousParameters().volume != 1.0);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.Native", voice.native);

  if (!voice.native) {
#if !defined(OS_ANDROID)
    DCHECK(!voice.engine_id.empty());
    current_utterance_ = utterance;
    utterance->SetEngineId(voice.engine_id);
    if (GetTtsControllerDelegate()->GetTtsEngineDelegate())
      GetTtsControllerDelegate()->GetTtsEngineDelegate()->Speak(utterance,
                                                                voice);
    bool sends_end_event =
        voice.events.find(TTS_EVENT_END) != voice.events.end();
    if (!sends_end_event) {
      utterance->Finish();
      delete utterance;
      current_utterance_ = nullptr;
      SpeakNextUtterance();
    }
#endif
  } else {
    // It's possible for certain platforms to send start events immediately
    // during |speak|.
    current_utterance_ = utterance;
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->Speak(utterance->GetId(), utterance->GetText(),
                            utterance->GetLang(), voice,
                            utterance->GetContinuousParameters(),
                            base::BindOnce(&TtsControllerImpl::OnSpeakFinished,
                                           base::Unretained(this), utterance));
  }
}

void TtsControllerImpl::OnSpeakFinished(TtsUtterance* utterance, bool success) {
  if (!success)
    current_utterance_ = nullptr;

  // If the native voice wasn't able to process this speech, see if
  // the browser has built-in TTS that isn't loaded yet.
  if (!success &&
      GetTtsPlatform()->LoadBuiltInTtsEngine(utterance->GetBrowserContext())) {
    utterance_queue_.push(utterance);
    return;
  }

  if (!success) {
    utterance->OnTtsEvent(TTS_EVENT_ERROR, kInvalidCharIndex, kInvalidLength,
                          GetTtsPlatform()->GetError());
    delete utterance;
    return;
  }
}

void TtsControllerImpl::ClearUtteranceQueue(bool send_events) {
  while (!utterance_queue_.empty()) {
    TtsUtterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    if (send_events)
      utterance->OnTtsEvent(TTS_EVENT_CANCELLED, kInvalidCharIndex,
                            kInvalidLength, std::string());
    else
      utterance->Finish();
    delete utterance;
  }
}

void TtsControllerImpl::FinishCurrentUtterance() {
  if (current_utterance_) {
    if (!current_utterance_->IsFinished())
      current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                                     kInvalidLength, std::string());
    delete current_utterance_;
    current_utterance_ = nullptr;
  }
}

void TtsControllerImpl::SpeakNextUtterance() {
  if (paused_)
    return;

  // Start speaking the next utterance in the queue.  Keep trying in case
  // one fails but there are still more in the queue to try.
  while (!utterance_queue_.empty() && !current_utterance_) {
    TtsUtterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    SpeakNow(utterance);
  }
}

void TtsControllerImpl::UpdateUtteranceDefaults(TtsUtterance* utterance) {
  double rate = utterance->GetContinuousParameters().rate;
  double pitch = utterance->GetContinuousParameters().pitch;
  double volume = utterance->GetContinuousParameters().volume;
#if defined(OS_CHROMEOS)
  GetTtsControllerDelegate()->UpdateUtteranceDefaultsFromPrefs(utterance, &rate,
                                                               &pitch, &volume);
#else
  // Update pitch, rate and volume to defaults if not explicity set on
  // this utterance.
  if (rate == blink::kWebSpeechSynthesisDoublePrefNotSet)
    rate = blink::kWebSpeechSynthesisDefaultTextToSpeechRate;
  if (pitch == blink::kWebSpeechSynthesisDoublePrefNotSet)
    pitch = blink::kWebSpeechSynthesisDefaultTextToSpeechPitch;
  if (volume == blink::kWebSpeechSynthesisDoublePrefNotSet)
    volume = blink::kWebSpeechSynthesisDefaultTextToSpeechVolume;
#endif  // defined(OS_CHROMEOS)
  utterance->SetContinuousParameters(rate, pitch, volume);
}

TtsControllerDelegate* TtsControllerImpl::GetTtsControllerDelegate() {
  if (delegate_)
    return delegate_;
  if (GetContentClient() && GetContentClient()->browser()) {
    delegate_ = GetContentClient()->browser()->GetTtsControllerDelegate();
    return delegate_;
  }
  return nullptr;
}

void TtsControllerImpl::StripSSML(
    const std::string& utterance,
    base::OnceCallback<void(const std::string&)> on_ssml_parsed) {
  // Skip parsing and return if not xml.
  if (utterance.find("<?xml") == std::string::npos) {
    std::move(on_ssml_parsed).Run(utterance);
    return;
  }

  // Parse using safe, out-of-process Xml Parser.
  service_manager::Connector* connector = GetSystemConnector();
  DCHECK(connector);
  data_decoder::ParseXml(connector, utterance,
                         base::BindOnce(&TtsControllerImpl::StripSSMLHelper,
                                        utterance, std::move(on_ssml_parsed)));
}

// Called when ParseXml finishes.
// Uses parsed xml to build parsed utterance text.
void TtsControllerImpl::StripSSMLHelper(
    const std::string& utterance,
    base::OnceCallback<void(const std::string&)> on_ssml_parsed,
    std::unique_ptr<base::Value> value,
    const base::Optional<std::string>& error_message) {
  // Error checks.
  // If invalid xml, return original utterance text.
  if (!value || error_message) {
    std::move(on_ssml_parsed).Run(utterance);
    return;
  }

  std::string root_tag_name;
  data_decoder::GetXmlElementTagName(*value, &root_tag_name);
  // Root element must be <speak>.
  if (root_tag_name.compare("speak") != 0) {
    std::move(on_ssml_parsed).Run(utterance);
    return;
  }

  std::string parsed_text = "";
  // Change from unique_ptr to base::Value* so recursion will work.
  PopulateParsedText(&parsed_text, &(*value));

  // Run with parsed_text.
  std::move(on_ssml_parsed).Run(parsed_text);
}

void TtsControllerImpl::PopulateParsedText(std::string* parsed_text,
                                           const base::Value* element) {
  DCHECK(parsed_text);
  if (!element)
    return;
  // Add element's text if present.
  // Note: We don't use data_decoder::GetXmlElementText because it gets the text
  // of element's first child, not text of current element.
  const base::Value* text_value = element->FindKeyOfType(
      data_decoder::mojom::XmlParser::kTextKey, base::Value::Type::STRING);
  if (text_value)
    *parsed_text += text_value->GetString();

  const base::Value* children = data_decoder::GetXmlElementChildren(*element);
  if (!children || !children->is_list())
    return;

  for (size_t i = 0; i < children->GetList().size(); ++i) {
    // We need to iterate over all children because some text elements are
    // nested within other types of elements, such as <emphasis> tags.
    PopulateParsedText(parsed_text, &children->GetList()[i]);
  }
}

}  // namespace content
