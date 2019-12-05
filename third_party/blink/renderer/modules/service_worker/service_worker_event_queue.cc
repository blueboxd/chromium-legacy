// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_event_queue.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/stl_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

int NextEventId() {
  // Event id should not start from zero since HashMap in Blink requires
  // non-zero keys.
  static base::AtomicSequenceNumber s_event_id_sequence;
  int next_event_id = s_event_id_sequence.GetNext() + 1;
  CHECK_LT(next_event_id, std::numeric_limits<int>::max());
  return next_event_id;
}

}  // namespace

// static
constexpr base::TimeDelta ServiceWorkerEventQueue::kIdleDelay;
constexpr base::TimeDelta ServiceWorkerEventQueue::kEventTimeout;
constexpr base::TimeDelta ServiceWorkerEventQueue::kUpdateInterval;

ServiceWorkerEventQueue::StayAwakeToken::StayAwakeToken(
    base::WeakPtr<ServiceWorkerEventQueue> event_queue)
    : event_queue_(std::move(event_queue)) {
  DCHECK(event_queue_);
  event_queue_->num_of_stay_awake_tokens_++;
}

ServiceWorkerEventQueue::StayAwakeToken::~StayAwakeToken() {
  // If |event_queue_| has already been destroyed, it means the worker thread
  // has already been killed.
  if (!event_queue_)
    return;
  DCHECK_GT(event_queue_->num_of_stay_awake_tokens_, 0);
  event_queue_->num_of_stay_awake_tokens_--;

  if (!event_queue_->HasInflightEvent())
    event_queue_->OnNoInflightEvent();
}

ServiceWorkerEventQueue::ServiceWorkerEventQueue(
    base::RepeatingClosure idle_callback)
    : ServiceWorkerEventQueue(std::move(idle_callback),
                              base::DefaultTickClock::GetInstance()) {}

ServiceWorkerEventQueue::ServiceWorkerEventQueue(
    base::RepeatingClosure idle_callback,
    const base::TickClock* tick_clock)
    : idle_callback_(std::move(idle_callback)), tick_clock_(tick_clock) {}

ServiceWorkerEventQueue::~ServiceWorkerEventQueue() {
  in_dtor_ = true;
  // Abort all callbacks.
  for (auto& event : id_event_map_) {
    std::move(event.value->abort_callback)
        .Run(blink::mojom::ServiceWorkerEventStatus::ABORTED);
  }
}

void ServiceWorkerEventQueue::Start() {
  DCHECK(!timer_.IsRunning());
  // |idle_callback_| will be invoked if no event happens in |kIdleDelay|.
  if (!HasInflightEvent() && idle_time_.is_null())
    idle_time_ = tick_clock_->NowTicks() + kIdleDelay;
  timer_.Start(FROM_HERE, kUpdateInterval,
               WTF::BindRepeating(&ServiceWorkerEventQueue::UpdateStatus,
                                  WTF::Unretained(this)));
}

void ServiceWorkerEventQueue::EnqueueNormal(
    StartCallback start_callback,
    AbortCallback abort_callback,
    base::Optional<base::TimeDelta> custom_timeout) {
  EnqueueEvent(std::make_unique<Event>(
      Event::Type::Normal, std::move(start_callback), std::move(abort_callback),
      std::move(custom_timeout)));
}

void ServiceWorkerEventQueue::EnqueuePending(
    StartCallback start_callback,
    AbortCallback abort_callback,
    base::Optional<base::TimeDelta> custom_timeout) {
  EnqueueEvent(std::make_unique<Event>(
      Event::Type::Pending, std::move(start_callback),
      std::move(abort_callback), std::move(custom_timeout)));
}

void ServiceWorkerEventQueue::EnqueueOffline(
    StartCallback start_callback,
    AbortCallback abort_callback,
    base::Optional<base::TimeDelta> custom_timeout) {
  EnqueueEvent(std::make_unique<ServiceWorkerEventQueue::Event>(
      ServiceWorkerEventQueue::Event::Type::Offline, std::move(start_callback),
      std::move(abort_callback), std::move(custom_timeout)));
}

bool ServiceWorkerEventQueue::CanStartEvent(const Event& event) const {
  if (!HasInflightEvent())
    return true;
  if (event.type == Event::Type::Offline)
    return running_offline_events_;
  return !running_offline_events_;
}

void ServiceWorkerEventQueue::EnqueueEvent(std::unique_ptr<Event> event) {
  DCHECK(event->type != Event::Type::Pending || did_idle_timeout());
  bool can_start_processing_events =
      !processing_events_ && event->type != Event::Type::Pending;
  queue_.emplace_back(std::move(event));
  if (!can_start_processing_events)
    return;
  if (did_idle_timeout()) {
    idle_time_ = base::TimeTicks();
    did_idle_timeout_ = false;
  }
  ProcessEvents();
}

void ServiceWorkerEventQueue::ProcessEvents() {
  DCHECK(!processing_events_);
  processing_events_ = true;
  while (!queue_.IsEmpty() && CanStartEvent(*queue_.front())) {
    StartEvent(queue_.TakeFirst());
  }
  processing_events_ = false;

  // We have to check HasInflightEvent() and may trigger
  // OnNoInflightEvent() here because StartEvent() can call EndEvent()
  // synchronously, and EndEvent() never triggers OnNoInflightEvent()
  // while ProcessEvents() is running.
  if (!HasInflightEvent())
    OnNoInflightEvent();
}

void ServiceWorkerEventQueue::StartEvent(std::unique_ptr<Event> event) {
  DCHECK(CanStartEvent(*event));
  running_offline_events_ = event->type == Event::Type::Offline;
  idle_time_ = base::TimeTicks();
  const int event_id = NextEventId();
  DCHECK(!HasEvent(event_id));
  id_event_map_.insert(
      event_id, std::make_unique<EventInfo>(
                    tick_clock_->NowTicks() +
                        event->custom_timeout.value_or(kEventTimeout),
                    WTF::Bind(std::move(event->abort_callback), event_id)));
  std::move(event->start_callback).Run(event_id);
}

void ServiceWorkerEventQueue::EndEvent(int event_id) {
  DCHECK(HasEvent(event_id));
  id_event_map_.erase(event_id);
  // Check |processing_events_| here because EndEvent() can be called
  // synchronously in StartEvent(). We don't want to trigger
  // OnNoInflightEvent() while ProcessEvents() is running.
  if (!processing_events_ && !HasInflightEvent())
    OnNoInflightEvent();
}

bool ServiceWorkerEventQueue::HasEvent(int event_id) const {
  return id_event_map_.find(event_id) != id_event_map_.end();
}

std::unique_ptr<ServiceWorkerEventQueue::StayAwakeToken>
ServiceWorkerEventQueue::CreateStayAwakeToken() {
  return std::make_unique<ServiceWorkerEventQueue::StayAwakeToken>(
      weak_factory_.GetWeakPtr());
}

void ServiceWorkerEventQueue::SetIdleTimerDelayToZero() {
  zero_idle_timer_delay_ = true;
  if (!HasInflightEvent())
    MaybeTriggerIdleTimer();
}

void ServiceWorkerEventQueue::UpdateStatus() {
  base::TimeTicks now = tick_clock_->NowTicks();

  HashMap<int /* event_id */, std::unique_ptr<EventInfo>> new_id_event_map;

  // Abort all events exceeding |kEventTimeout|.
  for (auto& it : id_event_map_) {
    auto& event_info = it.value;
    if (event_info->expiration_time > now) {
      new_id_event_map.insert(it.key, std::move(event_info));
      continue;
    }
    std::move(event_info->abort_callback)
        .Run(blink::mojom::ServiceWorkerEventStatus::TIMEOUT);
    // Shut down the worker as soon as possible since the worker may have gone
    // into bad state.
    zero_idle_timer_delay_ = true;
  }
  id_event_map_.swap(new_id_event_map);

  // If the worker is now idle, set the |idle_time_| and possibly trigger the
  // idle callback.
  if (!HasInflightEvent() && idle_time_.is_null()) {
    OnNoInflightEvent();
    return;
  }

  if (!idle_time_.is_null() && idle_time_ < now) {
    did_idle_timeout_ = true;
    idle_callback_.Run();
  }
}

bool ServiceWorkerEventQueue::MaybeTriggerIdleTimer() {
  DCHECK(!HasInflightEvent());
  if (!zero_idle_timer_delay_)
    return false;

  did_idle_timeout_ = true;
  idle_callback_.Run();
  return true;
}

void ServiceWorkerEventQueue::OnNoInflightEvent() {
  DCHECK(!HasInflightEvent());
  running_offline_events_ = false;
  // There might be events in the queue because offline (or non-offline) events
  // can be enqueued during running non-offline (or offline) events.
  if (!queue_.IsEmpty()) {
    ProcessEvents();
    return;
  }
  idle_time_ = tick_clock_->NowTicks() + kIdleDelay;
  MaybeTriggerIdleTimer();
}

bool ServiceWorkerEventQueue::HasInflightEvent() const {
  return !id_event_map_.IsEmpty() || num_of_stay_awake_tokens_ > 0;
}

ServiceWorkerEventQueue::Event::Event(
    ServiceWorkerEventQueue::Event::Type type,
    StartCallback start_callback,
    AbortCallback abort_callback,
    base::Optional<base::TimeDelta> custom_timeout)
    : type(type),
      start_callback(std::move(start_callback)),
      abort_callback(std::move(abort_callback)),
      custom_timeout(custom_timeout) {}

ServiceWorkerEventQueue::Event::~Event() = default;

ServiceWorkerEventQueue::EventInfo::EventInfo(
    base::TimeTicks expiration_time,
    base::OnceCallback<void(blink::mojom::ServiceWorkerEventStatus)>
        abort_callback)
    : expiration_time(expiration_time),
      abort_callback(std::move(abort_callback)) {}

ServiceWorkerEventQueue::EventInfo::~EventInfo() = default;

}  // namespace blink
