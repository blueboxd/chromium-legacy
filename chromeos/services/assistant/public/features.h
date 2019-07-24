// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_FEATURES_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace chromeos {
namespace assistant {
namespace features {

// Enable Assistant Feedback UI.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantAudioEraser;

// Enable Assistant Feedback UI.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantFeedbackUi;

// Enables Assistant warmer welcome.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantWarmerWelcomeFeature;

// Enables Assistant app support.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantAppSupport;

// Enables Assistant routines.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantRoutines;

// Enables in-Assistant notifications.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kInAssistantNotifications;

// Enables clear cut logging.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableClearCutLog;

// Enables DSP for hotword detection.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableDspHotword;

// Enables MediaSession Integration.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableMediaSessionIntegration;

// Enables stereo audio input.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableStereoAudioInput;

// Enables timer notifications.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kTimerNotification;

// Enables Assistant alarm timer manager integration.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableAssistantAlarmTimerManager;

// Enables power management features i.e. Wake locks and wake up alarms.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnablePowerManager;

// Enables sending the client discourse context with text queries.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableTextQueriesWithClientDiscourseContext;

// Enables timer ticks. This feature causes alarms/timers tracked by the
// AssistantAlarmTimerController to tick at a fixed interval, delivering updates
// to AssistantAlarmTimerModelObservers of time remaining/elapsed since expiry.
// When enabled in conjunction with |kTimerNotification|, Assistant alarm/timer
// notifications will be updated at each tick.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kTimerTicks;

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsAlarmTimerManagerEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsAppSupportEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsAudioEraserEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsClearCutLogEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsDspHotwordEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsFeedbackUiEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsInAssistantNotificationsEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsKeyRemappingEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsMediaSessionIntegrationEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsPowerManagerEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsRoutinesEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsScreenContextQueryEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsStereoAudioInputEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsTimerNotificationEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsTimerTicksEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsWarmerWelcomeEnabled();

}  // namespace features
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_FEATURES_H_
