// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_VIEW_DELEGATE_H_
#define ASH_ASSISTANT_UI_ASSISTANT_VIEW_DELEGATE_H_

#include <map>
#include <string>

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/image_downloader.h"
#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

class AssistantAlarmTimerModel;
class AssistantAlarmTimerModelObserver;
class AssistantNotificationModel;
class AssistantNotificationModelObserver;
enum class AssistantButtonId;

namespace assistant {
namespace util {
enum class DeepLinkType;
}  // namespace util
}  // namespace assistant

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantViewDelegateObserver
    : public base::CheckedObserver {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

  // Invoked when the dialog plate button identified by |id| is pressed.
  virtual void OnDialogPlateButtonPressed(AssistantButtonId id) {}

  // Invoked when the dialog plate contents have been committed.
  virtual void OnDialogPlateContentsCommitted(const std::string& text) {}

  // Invoked when the host view's visibility changed.
  virtual void OnHostViewVisibilityChanged(bool visible) {}

  // Invoked when the opt in button is pressed.
  virtual void OnOptInButtonPressed() {}

  // Invoked when the proactive suggestions close button is pressed.
  virtual void OnProactiveSuggestionsCloseButtonPressed() {}

  // Invoked when the hover state of the proactive suggestions view is changed.
  virtual void OnProactiveSuggestionsViewHoverChanged(bool is_hovering) {}

  // Invoked when the proactive suggestions view is pressed.
  virtual void OnProactiveSuggestionsViewPressed() {}

  // Invoked when a suggestion chip is pressed.
  virtual void OnSuggestionChipPressed(const AssistantSuggestion* suggestion) {}
};

// A delegate of views in assistant/ui that handles views related actions e.g.
// get models for the views, adding observers, closing the views, opening urls,
// etc.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantViewDelegate {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

  virtual ~AssistantViewDelegate() {}

  // Gets the alarm/timer model.
  virtual const AssistantAlarmTimerModel* GetAlarmTimerModel() const = 0;

  // Gets the notification model.
  virtual const AssistantNotificationModel* GetNotificationModel() const = 0;

  // Adds/removes the specified view delegate observer.
  virtual void AddObserver(AssistantViewDelegateObserver* observer) = 0;
  virtual void RemoveObserver(AssistantViewDelegateObserver* observer) = 0;

  // Adds/removes the specified alarm/timer model observer.
  virtual void AddAlarmTimerModelObserver(
      AssistantAlarmTimerModelObserver* observer) = 0;
  virtual void RemoveAlarmTimerModelObserver(
      AssistantAlarmTimerModelObserver* observer) = 0;

  // Adds/removes the notification model observer.
  virtual void AddNotificationModelObserver(
      AssistantNotificationModelObserver* observer) = 0;
  virtual void RemoveNotificationModelObserver(
      AssistantNotificationModelObserver* observer) = 0;

  // Downloads the image found at the specified |url|. On completion, the
  // supplied |callback| will be run with the downloaded image. If the download
  // attempt is unsuccessful, a NULL image is returned.
  virtual void DownloadImage(const GURL& url,
                             ImageDownloader::DownloadCallback callback) = 0;

  // Returns the cursor_manager.
  virtual ::wm::CursorManager* GetCursorManager() = 0;

  // Returns the root window for the specified |display_id|.
  virtual aura::Window* GetRootWindowForDisplayId(int64_t display_id) = 0;

  // Returns the root window that newly created windows should be added to.
  virtual aura::Window* GetRootWindowForNewWindows() = 0;

  // Returns true if in tablet mode.
  virtual bool IsTabletMode() const = 0;

  // Invoked when the dialog plate button identified by |id| is pressed.
  virtual void OnDialogPlateButtonPressed(AssistantButtonId id) = 0;

  // Invoked when the dialog plate contents have been committed.
  virtual void OnDialogPlateContentsCommitted(const std::string& text) = 0;

  // Invoked when the host view's visibility changed.
  virtual void OnHostViewVisibilityChanged(bool visible) = 0;

  // Invoked when an in-Assistant notification button is pressed.
  virtual void OnNotificationButtonPressed(const std::string& notification_id,
                                           int notification_button_index) = 0;

  // Invoked when the opt in button is pressed.
  virtual void OnOptInButtonPressed() {}

  // Invoked when the proactive suggestions close button is pressed.
  virtual void OnProactiveSuggestionsCloseButtonPressed() {}

  // Invoked when the hover state of the proactive suggestions view is changed.
  virtual void OnProactiveSuggestionsViewHoverChanged(bool is_hovering) {}

  // Invoked when the proactive suggestions view is pressed.
  virtual void OnProactiveSuggestionsViewPressed() {}

  // Invoked when suggestion chip is pressed.
  virtual void OnSuggestionChipPressed(
      const AssistantSuggestion* suggestion) = 0;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_VIEW_DELEGATE_H_
