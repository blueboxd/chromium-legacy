// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/font_preload_manager.h"

#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_finish_observer.h"

namespace blink {

namespace {

class FontPreloadFinishObserver final : public ResourceFinishObserver {
 public:
  FontPreloadFinishObserver(FontResource& font_resource, Document& document)
      : font_resource_(font_resource), document_(document) {}

  ~FontPreloadFinishObserver() final = default;

  void Trace(blink::Visitor* visitor) final {
    visitor->Trace(font_resource_);
    visitor->Trace(document_);
    ResourceFinishObserver::Trace(visitor);
  }

 private:
  void NotifyFinished() final {
    document_->GetFontPreloadManager().FontPreloadingFinished(font_resource_,
                                                              this);
  }

  String DebugName() const final { return "FontPreloadFinishObserver"; }

  Member<FontResource> font_resource_;
  Member<Document> document_;
};

}  // namespace

FontPreloadManager::FontPreloadManager(Document& document)
    : document_(document),
      render_delay_timer_(
          document.GetTaskRunner(TaskType::kInternalFrameLifecycleControl),
          this,
          &FontPreloadManager::FontPreloadingDelaysRenderingTimerFired),
      render_delay_timeout_(base::TimeDelta::FromMilliseconds(
          features::kFontPreloadingDelaysRenderingParam.Get())) {}

bool FontPreloadManager::HasPendingRenderBlockingFonts() const {
  return state_ == State::kLoading;
}

void FontPreloadManager::FontPreloadingStarted(FontResource* font_resource) {
  if (!base::FeatureList::IsEnabled(features::kFontPreloadingDelaysRendering))
    return;

  // The font is either already in the memory cache, or has errored out. In
  // either case, we don't any further processing.
  if (font_resource->IsLoaded())
    return;

  switch (state_) {
    case State::kInitial:
      render_delay_timer_.StartOneShot(render_delay_timeout_, FROM_HERE);
      FALLTHROUGH;
    case State::kLoading:
    case State::kLoaded: {
      FontPreloadFinishObserver* observer =
          MakeGarbageCollected<FontPreloadFinishObserver>(*font_resource,
                                                          *document_);
      font_resource->AddFinishObserver(
          observer, document_->GetTaskRunner(TaskType::kInternalLoading).get());
      finish_observers_.insert(observer);
      state_ = State::kLoading;
      break;
    }
    case State::kUnblocked:
      break;
    default:
      NOTREACHED();
  }
}

void FontPreloadManager::FontPreloadingFinished(
    FontResource* font_resource,
    ResourceFinishObserver* observer) {
  DCHECK(
      base::FeatureList::IsEnabled(features::kFontPreloadingDelaysRendering));
  if (state_ == State::kUnblocked) {
    finish_observers_.clear();
    return;
  }

  DCHECK(finish_observers_.Contains(observer));
  finish_observers_.erase(observer);
  if (!finish_observers_.IsEmpty())
    return;

  state_ = State::kLoaded;
  document_->FontPreloadingFinishedOrTimedOut();
}

void FontPreloadManager::WillBeginRendering() {
  if (!base::FeatureList::IsEnabled(features::kFontPreloadingDelaysRendering))
    return;
  if (state_ == State::kUnblocked)
    return;

  state_ = State::kUnblocked;
  finish_observers_.clear();
}

void FontPreloadManager::FontPreloadingDelaysRenderingTimerFired(TimerBase*) {
  if (state_ == State::kUnblocked)
    return;

  WillBeginRendering();
  document_->FontPreloadingFinishedOrTimedOut();
}

void FontPreloadManager::SetRenderDelayTimeoutForTest(base::TimeDelta timeout) {
  if (render_delay_timer_.IsActive()) {
    render_delay_timer_.Stop();
    render_delay_timer_.StartOneShot(timeout, FROM_HERE);
  }
  render_delay_timeout_ = timeout;
}

void FontPreloadManager::Trace(Visitor* visitor) {
  visitor->Trace(finish_observers_);
  visitor->Trace(document_);
}

}  // namespace blink
