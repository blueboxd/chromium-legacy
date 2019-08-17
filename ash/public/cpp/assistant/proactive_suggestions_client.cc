// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/proactive_suggestions_client.h"

namespace ash {

namespace {

ProactiveSuggestionsClient* g_instance = nullptr;

}  // namespace

// static
ProactiveSuggestionsClient* ProactiveSuggestionsClient::GetInstance() {
  return g_instance;
}

ProactiveSuggestionsClient::ProactiveSuggestionsClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ProactiveSuggestionsClient::~ProactiveSuggestionsClient() {
  for (auto& observer : observers_)
    observer.OnProactiveSuggestionsClientDestroying();

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void ProactiveSuggestionsClient::AddObserver(
    ProactiveSuggestionsClientObserver* observer) {
  observers_.AddObserver(observer);
}

void ProactiveSuggestionsClient::RemoveObserver(
    ProactiveSuggestionsClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
