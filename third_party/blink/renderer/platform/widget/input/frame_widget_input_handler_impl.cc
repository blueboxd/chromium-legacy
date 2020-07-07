// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/frame_widget_input_handler_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/widget/input/ime_event_guard.h"
#include "third_party/blink/renderer/platform/widget/input/main_thread_event_queue.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"

namespace blink {

FrameWidgetInputHandlerImpl::FrameWidgetInputHandlerImpl(
    base::WeakPtr<WidgetBase> widget,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    scoped_refptr<MainThreadEventQueue> input_event_queue)
    : widget_(widget),
      input_event_queue_(input_event_queue),
      main_thread_task_runner_(main_thread_task_runner) {}

FrameWidgetInputHandlerImpl::~FrameWidgetInputHandlerImpl() = default;

void FrameWidgetInputHandlerImpl::RunOnMainThread(base::OnceClosure closure) {
  if (input_event_queue_) {
    input_event_queue_->QueueClosure(std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

void FrameWidgetInputHandlerImpl::AddImeTextSpansToExistingText(
    uint32_t start,
    uint32_t end,
    const Vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, uint32_t start, uint32_t end,
         const Vector<ui::ImeTextSpan>& ui_ime_text_spans) {
        if (!widget)
          return;

        ImeEventGuard guard(widget);
        widget->client()->FrameWidget()->AddImeTextSpansToExistingText(
            start, end, ui_ime_text_spans);
      },
      widget_, start, end, ui_ime_text_spans));
}

void FrameWidgetInputHandlerImpl::ClearImeTextSpansByType(
    uint32_t start,
    uint32_t end,
    ui::ImeTextSpan::Type type) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, uint32_t start, uint32_t end,
         ui::ImeTextSpan::Type type) {
        if (!widget)
          return;

        ImeEventGuard guard(widget);
        widget->client()->FrameWidget()->ClearImeTextSpansByType(start, end,
                                                                 type);
      },
      widget_, start, end, type));
}

void FrameWidgetInputHandlerImpl::SetCompositionFromExistingText(
    int32_t start,
    int32_t end,
    const Vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, int32_t start, int32_t end,
         const Vector<ui::ImeTextSpan>& ui_ime_text_spans) {
        if (!widget)
          return;

        ImeEventGuard guard(widget);

        widget->client()->FrameWidget()->SetCompositionFromExistingText(
            start, end, ui_ime_text_spans);
      },
      widget_, start, end, ui_ime_text_spans));
}

void FrameWidgetInputHandlerImpl::ExtendSelectionAndDelete(int32_t before,
                                                           int32_t after) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, int32_t before, int32_t after) {
        if (!widget)
          return;
        widget->client()->FrameWidget()->ExtendSelectionAndDelete(before,
                                                                  after);
      },
      widget_, before, after));
}

void FrameWidgetInputHandlerImpl::DeleteSurroundingText(int32_t before,
                                                        int32_t after) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, int32_t before, int32_t after) {
        if (!widget)
          return;

        if (!widget)
          return;
        widget->client()->FrameWidget()->DeleteSurroundingText(before, after);
      },
      widget_, before, after));
}

void FrameWidgetInputHandlerImpl::DeleteSurroundingTextInCodePoints(
    int32_t before,
    int32_t after) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, int32_t before, int32_t after) {
        if (!widget)
          return;

        widget->client()->FrameWidget()->DeleteSurroundingTextInCodePoints(
            before, after);
      },
      widget_, before, after));
}

void FrameWidgetInputHandlerImpl::SetEditableSelectionOffsets(int32_t start,
                                                              int32_t end) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, int32_t start, int32_t end) {
        if (!widget)
          return;

        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        widget->client()->FrameWidget()->SetEditableSelectionOffsets(start,
                                                                     end);
      },
      widget_, start, end));
}

void FrameWidgetInputHandlerImpl::ExecuteEditCommand(const String& command,
                                                     const String& value) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, const String& command,
         const String& value) {
        if (!widget)
          return;

        widget->client()->FrameWidget()->ExecuteEditCommand(command, value);
      },
      widget_, command.IsolatedCopy(), value.IsolatedCopy()));
}

void FrameWidgetInputHandlerImpl::Undo() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "Undo", UpdateState::kNone));
}

void FrameWidgetInputHandlerImpl::Redo() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "Redo", UpdateState::kNone));
}

void FrameWidgetInputHandlerImpl::Cut() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "Cut", UpdateState::kIsSelectingRange));
}

void FrameWidgetInputHandlerImpl::Copy() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "Copy", UpdateState::kIsSelectingRange));
}

void FrameWidgetInputHandlerImpl::CopyToFindPboard() {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget) {
        if (!widget)
          return;

        widget->client()->FrameWidget()->CopyToFindPboard();
      },
      widget_));
}

void FrameWidgetInputHandlerImpl::Paste() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "Paste", UpdateState::kIsPasting));
}

void FrameWidgetInputHandlerImpl::PasteAndMatchStyle() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "PasteAndMatchStyle", UpdateState::kIsPasting));
}

void FrameWidgetInputHandlerImpl::Replace(const String& word) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, const String& word) {
        if (!widget)
          return;

        widget->client()->FrameWidget()->Replace(word);
      },
      widget_, word.IsolatedCopy()));
}

void FrameWidgetInputHandlerImpl::ReplaceMisspelling(const String& word) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, const String& word) {
        if (!widget)
          return;

        widget->client()->FrameWidget()->ReplaceMisspelling(word);
      },
      widget_, word.IsolatedCopy()));
}

void FrameWidgetInputHandlerImpl::Delete() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "Delete", UpdateState::kNone));
}

void FrameWidgetInputHandlerImpl::SelectAll() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "SelectAll", UpdateState::kIsSelectingRange));
}

void FrameWidgetInputHandlerImpl::CollapseSelection() {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget) {
        if (!widget)
          return;

        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        widget->client()->FrameWidget()->CollapseSelection();
      },
      widget_));
}

void FrameWidgetInputHandlerImpl::SelectRange(const gfx::Point& base,
                                              const gfx::Point& extent) {
  // TODO(dtapuska): This event should be coalesced. Chrome IPC uses
  // one outstanding event and an ACK to handle coalescing on the browser
  // side. We should be able to clobber them in the main thread event queue.
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, const gfx::Point& base,
         const gfx::Point& extent) {
        if (!widget)
          return;

        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        widget->client()->FrameWidget()->SelectRange(base, extent);
      },
      widget_, base, extent));
}

#if defined(OS_ANDROID)

void FrameWidgetInputHandlerImpl::SelectWordAroundCaret(
    SelectWordAroundCaretCallback callback) {
  // If the mojom channel is registered with compositor thread, we have to run
  // the callback on compositor thread. Otherwise run it on main thread. Mojom
  // requires the callback runs on the same thread.
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    callback = base::BindOnce(
        [](scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner,
           SelectWordAroundCaretCallback callback, bool did_select,
           int start_adjust, int end_adjust) {
          callback_task_runner->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback), did_select,
                                        start_adjust, end_adjust));
        },
        base::ThreadTaskRunnerHandle::Get(), std::move(callback));
  }

  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget,
         SelectWordAroundCaretCallback callback) {
        if (widget) {
          FrameWidget* frame_widget = widget->client()->FrameWidget();
          if (frame_widget)
            frame_widget->SelectWordAroundCaret(std::move(callback));
        }
        if (callback)
          std::move(callback).Run(false, 0, 0);
      },
      widget_, std::move(callback)));
}
#endif  // defined(OS_ANDROID)

void FrameWidgetInputHandlerImpl::AdjustSelectionByCharacterOffset(
    int32_t start,
    int32_t end,
    blink::mojom::SelectionMenuBehavior selection_menu_behavior) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, int32_t start, int32_t end,
         blink::mojom::SelectionMenuBehavior selection_menu_behavior) {
        if (!widget)
          return;

        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        widget->client()->FrameWidget()->AdjustSelectionByCharacterOffset(
            start, end, selection_menu_behavior);
      },
      widget_, start, end, selection_menu_behavior));
}

void FrameWidgetInputHandlerImpl::MoveRangeSelectionExtent(
    const gfx::Point& extent) {
  // TODO(dtapuska): This event should be coalesced. Chrome IPC uses
  // one outstanding event and an ACK to handle coalescing on the browser
  // side. We should be able to clobber them in the main thread event queue.
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, const gfx::Point& extent) {
        if (!widget)
          return;

        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        widget->client()->FrameWidget()->MoveRangeSelectionExtent(extent);
      },
      widget_, extent));
}

void FrameWidgetInputHandlerImpl::ScrollFocusedEditableNodeIntoRect(
    const gfx::Rect& rect) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, const gfx::Rect& rect) {
        if (!widget)
          return;

        widget->client()->FrameWidget()->ScrollFocusedEditableNodeIntoRect(
            rect);
      },
      widget_, rect));
}

void FrameWidgetInputHandlerImpl::MoveCaret(const gfx::Point& point) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget, const gfx::Point& point) {
        if (!widget)
          return;

        widget->client()->FrameWidget()->MoveCaret(point);
      },
      widget_, point));
}

void FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread(
    base::WeakPtr<WidgetBase> widget,
    const char* command,
    UpdateState update_state) {
  if (!widget)
    return;

  HandlingState handling_state(widget, update_state);
  widget->client()->FrameWidget()->ExecuteEditCommand(command, String());
}

FrameWidgetInputHandlerImpl::HandlingState::HandlingState(
    const base::WeakPtr<WidgetBase>& widget,
    UpdateState state)
    : widget_(widget),
      original_select_range_value_(widget->handling_select_range()),
      original_pasting_value_(widget->is_pasting()) {
  switch (state) {
    case UpdateState::kIsPasting:
      widget->set_is_pasting(true);
      FALLTHROUGH;  // Set both
    case UpdateState::kIsSelectingRange:
      widget->set_handling_select_range(true);
      break;
    case UpdateState::kNone:
      break;
  }
}

FrameWidgetInputHandlerImpl::HandlingState::~HandlingState() {
  // FrameWidget may have been destroyed while this object was on the stack.
  if (!widget_)
    return;
  widget_->set_handling_select_range(original_select_range_value_);
  widget_->set_is_pasting(original_pasting_value_);
}

}  // namespace blink
