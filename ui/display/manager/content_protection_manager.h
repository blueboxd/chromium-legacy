// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CONTENT_PROTECTION_MANAGER_H_
#define UI_DISPLAY_MANAGER_CONTENT_PROTECTION_MANAGER_H_

#include <cstdint>
#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager_export.h"

namespace display {

class DisplayLayoutManager;
class DisplaySnapshot;
class NativeDisplayDelegate;

namespace test {
class ContentProtectionManagerTest;
}  // namespace test

// Fulfills client requests to query and apply per-display content protection.
class DISPLAY_MANAGER_EXPORT ContentProtectionManager
    : public DisplayConfigurator::Observer {
 public:
  // |connection_mask| is a DisplayConnectionType bitmask, and |protection_mask|
  // is a ContentProtectionMethod bitmask.
  using QueryContentProtectionCallback = base::OnceCallback<
      void(bool success, uint32_t connection_mask, uint32_t protection_mask)>;
  using ApplyContentProtectionCallback = base::OnceCallback<void(bool success)>;

  using ContentProtections =
      base::flat_map<int64_t /* display_id */, uint32_t /* protection_mask */>;

  // Though only run once, a task must outlive its asynchronous operations, so
  // cannot be a OnceCallback.
  struct Task {
    enum class Status { KILLED, FAILURE, SUCCESS };

    virtual ~Task() = default;
    virtual void Run() = 0;
  };

  // Returns whether display configuration is disabled, in which case API calls
  // are no-ops resulting in failure callbacks.
  using ConfigurationDisabledCallback = base::RepeatingCallback<bool()>;

  ContentProtectionManager(DisplayLayoutManager*,
                           ConfigurationDisabledCallback);
  ~ContentProtectionManager() override;

  void set_native_display_delegate(NativeDisplayDelegate* delegate) {
    native_display_delegate_ = delegate;
  }

  using ClientId = base::Optional<uint64_t>;

  // On display reconfiguration, pending requests are cancelled, i.e. clients
  // receive failure callbacks, and are responsible for renewing requests. If a
  // client unregisters with pending requests, the callbacks are not run.
  ClientId RegisterClient();
  void UnregisterClient(ClientId client_id);

  // Queries protection against the client's latest request on the same display,
  // i.e. the result is CONTENT_PROTECTION_METHOD_NONE unless the client has
  // previously applied protection on that display, such that requests from
  // other clients are concealed.
  void QueryContentProtection(ClientId client_id,
                              int64_t display_id,
                              QueryContentProtectionCallback callback);

  // |protection_mask| is a ContentProtectionMethod bitmask. Callback success
  // does not mean that protection is active, but merely that the request went
  // through. The client must periodically query protection status until it no
  // longer requires protection and applies CONTENT_PROTECTION_METHOD_NONE. If
  // protection becomes temporarily unavailable, the client is not required to
  // renew the request, but should keep querying to detect if automatic retries
  // to establish protection are successful.
  void ApplyContentProtection(ClientId client_id,
                              int64_t display_id,
                              uint32_t protection_mask,
                              ApplyContentProtectionCallback callback);

 private:
  friend class test::ContentProtectionManagerTest;

  bool disabled() const {
    return !native_display_delegate_ || config_disabled_callback_.Run();
  }

  const DisplaySnapshot* GetDisplay(int64_t display_id) const;

  // Returns cumulative content protections given all client requests.
  ContentProtections AggregateContentProtections() const;

  // Returns content protections for |client_id|, or nullptr if invalid.
  ContentProtections* GetContentProtections(ClientId client_id);

  void QueueTask(std::unique_ptr<Task> task);
  void DequeueTask();
  void KillTasks();

  // Called on task completion. Responsible for running the client callback, and
  // dequeuing the next pending task.
  void OnContentProtectionQueried(QueryContentProtectionCallback callback,
                                  ClientId client_id,
                                  int64_t display_id,
                                  Task::Status status,
                                  uint32_t connection_mask,
                                  uint32_t protection_mask);
  void OnContentProtectionApplied(ApplyContentProtectionCallback callback,
                                  ClientId client_id,
                                  Task::Status status);

  // DisplayConfigurator::Observer overrides:
  void OnDisplayModeChanged(
      const DisplayConfigurator::DisplayStateList&) override;
  void OnDisplayModeChangeFailed(const DisplayConfigurator::DisplayStateList&,
                                 MultipleDisplayState) override;

  DisplayLayoutManager* const layout_manager_;                // Not owned.
  NativeDisplayDelegate* native_display_delegate_ = nullptr;  // Not owned.

  const ConfigurationDisabledCallback config_disabled_callback_;

  uint64_t next_client_id_ = 0;

  // Content protections requested by each client.
  base::flat_map<uint64_t, ContentProtections> requests_;

  // Pending tasks to query or apply content protection.
  base::queue<std::unique_ptr<Task>> tasks_;

  base::WeakPtrFactory<ContentProtectionManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ContentProtectionManager);
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CONTENT_PROTECTION_MANAGER_H_
