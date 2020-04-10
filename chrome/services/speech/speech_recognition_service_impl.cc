// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_recognition_service_impl.h"

#include "chrome/services/speech/speech_recognition_recognizer_impl.h"

namespace speech {

SpeechRecognitionServiceImpl::SpeechRecognitionServiceImpl(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionService> receiver)
    : receiver_(this, std::move(receiver)) {}

SpeechRecognitionServiceImpl::~SpeechRecognitionServiceImpl() = default;

void SpeechRecognitionServiceImpl::BindContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> context) {
  speech_recognition_contexts_.Add(this, std::move(context));
}

void SpeechRecognitionServiceImpl::BindRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
        client) {
  SpeechRecognitionRecognizerImpl::Create(std::move(receiver),
                                          std::move(client));
}

}  // namespace speech
