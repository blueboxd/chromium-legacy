// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/frame_input_handler_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/widget_input_handler_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

FrameInputHandlerImpl::FrameInputHandlerImpl(
    base::WeakPtr<RenderWidget> widget,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    scoped_refptr<MainThreadEventQueue> input_event_queue)
    : widget_(widget),
      input_event_queue_(input_event_queue),
      main_thread_task_runner_(main_thread_task_runner) {}

FrameInputHandlerImpl::~FrameInputHandlerImpl() {}

void FrameInputHandlerImpl::RunOnMainThread(base::OnceClosure closure) {
  if (input_event_queue_) {
    input_event_queue_->QueueClosure(std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

void FrameInputHandlerImpl::SetCompositionFromExistingText(
    int32_t start,
    int32_t end,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget, int32_t start, int32_t end,
         const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
        if (!widget)
          return;

        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;
        ImeEventGuard guard(widget);

        focused_frame->SetCompositionFromExistingText(start, end,
                                                      ui_ime_text_spans);
      },
      widget_, start, end, ui_ime_text_spans));
}

void FrameInputHandlerImpl::ExtendSelectionAndDelete(int32_t before,
                                                     int32_t after) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget, int32_t before, int32_t after) {
        if (!widget)
          return;
        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;
        focused_frame->ExtendSelectionAndDelete(before, after);
      },
      widget_, before, after));
}

void FrameInputHandlerImpl::DeleteSurroundingText(int32_t before,
                                                  int32_t after) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget, int32_t before, int32_t after) {
        if (!widget)
          return;

        if (!widget)
          return;
        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;
        focused_frame->DeleteSurroundingText(before, after);
      },
      widget_, before, after));
}

void FrameInputHandlerImpl::DeleteSurroundingTextInCodePoints(int32_t before,
                                                              int32_t after) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget, int32_t before, int32_t after) {
        if (!widget)
          return;

        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;
        focused_frame->DeleteSurroundingTextInCodePoints(before, after);
      },
      widget_, before, after));
}

void FrameInputHandlerImpl::SetEditableSelectionOffsets(int32_t start,
                                                        int32_t end) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget, int32_t start, int32_t end) {
        if (!widget)
          return;

        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;
        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        focused_frame->SetEditableSelectionOffsets(start, end);
      },
      widget_, start, end));
}

void FrameInputHandlerImpl::ExecuteEditCommand(
    const std::string& command,
    const base::Optional<base::string16>& value) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget, const std::string& command,
         const base::Optional<base::string16>& value) {
        if (!widget)
          return;

        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;

        if (value) {
          focused_frame->ExecuteCommand(
              blink::WebString::FromUTF8(command),
              blink::WebString::FromUTF16(value.value()));
          return;
        }

        focused_frame->ExecuteCommand(blink::WebString::FromUTF8(command));
      },
      widget_, command, value));
}

void FrameInputHandlerImpl::Undo() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "Undo", UpdateState::kNone));
}

void FrameInputHandlerImpl::Redo() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "Redo", UpdateState::kNone));
}

void FrameInputHandlerImpl::Cut() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "Cut", UpdateState::kIsSelectingRange));
}

void FrameInputHandlerImpl::Copy() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "Copy", UpdateState::kIsSelectingRange));
}

void FrameInputHandlerImpl::CopyToFindPboard() {
#if defined(OS_MACOSX)
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget) {
        if (!widget)
          return;

        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;

        RenderFrameImpl* render_frame =
            RenderFrameImpl::FromWebFrame(focused_frame);

        if (!render_frame)
          return;

        render_frame->OnCopyToFindPboard();
      },
      widget_));
#endif
}

void FrameInputHandlerImpl::Paste() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "Paste", UpdateState::kIsPasting));
}

void FrameInputHandlerImpl::PasteAndMatchStyle() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "PasteAndMatchStyle", UpdateState::kIsPasting));
}

void FrameInputHandlerImpl::Replace(const base::string16& word) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget, const base::string16& word) {
        if (!widget)
          return;

        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;

        RenderFrameImpl* render_frame =
            RenderFrameImpl::FromWebFrame(focused_frame);

        if (!render_frame)
          return;

        if (!focused_frame->HasSelection())
          focused_frame->SelectWordAroundCaret();
        focused_frame->ReplaceSelection(blink::WebString::FromUTF16(word));
        render_frame->SyncSelectionIfRequired();
      },
      widget_, word));
}

void FrameInputHandlerImpl::ReplaceMisspelling(const base::string16& word) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget, const base::string16& word) {
        if (!widget)
          return;

        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;
        if (!focused_frame->HasSelection())
          return;
        focused_frame->ReplaceMisspelledRange(
            blink::WebString::FromUTF16(word));
      },
      widget_, word));
}

void FrameInputHandlerImpl::Delete() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "Delete", UpdateState::kNone));
}

void FrameInputHandlerImpl::SelectAll() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, "SelectAll", UpdateState::kIsSelectingRange));
}

void FrameInputHandlerImpl::CollapseSelection() {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget) {
        if (!widget)
          return;

        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;
        const blink::WebRange& range =
            focused_frame->GetInputMethodController()->GetSelectionOffsets();
        if (range.IsNull())
          return;

        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        focused_frame->SelectRange(blink::WebRange(range.EndOffset(), 0),
                                   blink::WebLocalFrame::kHideSelectionHandle,
                                   blink::mojom::SelectionMenuBehavior::kHide);
      },
      widget_));
}

void FrameInputHandlerImpl::SelectRange(const gfx::Point& base,
                                        const gfx::Point& extent) {
    // TODO(dtapuska): This event should be coalesced. Chrome IPC uses
    // one outstanding event and an ACK to handle coalescing on the browser
    // side. We should be able to clobber them in the main thread event queue.
    RunOnMainThread(base::BindOnce(
        [](base::WeakPtr<RenderWidget> widget, const gfx::Point& base,
           const gfx::Point& extent) {
          if (!widget)
            return;

          auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
          if (!focused_frame)
            return;

          HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
          focused_frame->SelectRange(
              widget->ConvertWindowPointToViewport(base),
              widget->ConvertWindowPointToViewport(extent));
        },
        widget_, base, extent));
}

#if defined(OS_ANDROID)
void FrameInputHandlerImpl::SelectWordAroundCaret(
    SelectWordAroundCaretCallback callback) {
  // If the mojom channel is registered with compositor thread, we have to run
  // the callback on compositor thread. Otherwise run it on main thread. Mojom
  // requires the callback runs on the same thread.
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    callback = base::BindOnce(
        [](scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
           SelectWordAroundCaretCallback callback, bool did_select,
           int32_t start_adjust, int32_t end_adjust) {
          compositor_task_runner->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback), did_select,
                                        start_adjust, end_adjust));
        },
        base::ThreadTaskRunnerHandle::Get(), std::move(callback));
  }

  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget,
         SelectWordAroundCaretCallback callback) {
        if (!widget) {
          std::move(callback).Run(false, 0, 0);
          return;
        }
        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame) {
          std::move(callback).Run(false, 0, 0);
          return;
        }

        bool did_select = false;
        int start_adjust = 0;
        int end_adjust = 0;
        blink::WebRange initial_range = focused_frame->SelectionRange();
        widget->SetHandlingInputEvent(true);
        if (!initial_range.IsNull())
          did_select = focused_frame->SelectWordAroundCaret();
        if (did_select) {
          blink::WebRange adjusted_range = focused_frame->SelectionRange();
          DCHECK(!adjusted_range.IsNull());
          start_adjust =
              adjusted_range.StartOffset() - initial_range.StartOffset();
          end_adjust = adjusted_range.EndOffset() - initial_range.EndOffset();
        }
        widget->SetHandlingInputEvent(false);
        std::move(callback).Run(did_select, start_adjust, end_adjust);
      },
      widget_, std::move(callback)));
}
#endif  // defined(OS_ANDROID)

void FrameInputHandlerImpl::AdjustSelectionByCharacterOffset(
    int32_t start,
    int32_t end,
    blink::mojom::SelectionMenuBehavior selection_menu_behavior) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget, int32_t start, int32_t end,
         blink::mojom::SelectionMenuBehavior selection_menu_behavior) {
        if (!widget)
          return;

        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;

        blink::WebRange range =
            focused_frame->GetInputMethodController()->GetSelectionOffsets();
        if (range.IsNull())
          return;

        // Sanity checks to disallow empty and out of range selections.
        if (start - end > range.length() || range.StartOffset() + start < 0)
          return;

        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        // A negative adjust amount moves the selection towards the beginning of
        // the document, a positive amount moves the selection towards the end
        // of the document.
        focused_frame->SelectRange(
            blink::WebRange(range.StartOffset() + start,
                            range.length() + end - start),
            blink::WebLocalFrame::kPreserveHandleVisibility,
            selection_menu_behavior);
      },
      widget_, start, end, selection_menu_behavior));
}

void FrameInputHandlerImpl::MoveRangeSelectionExtent(const gfx::Point& extent) {
  // TODO(dtapuska): This event should be coalesced. Chrome IPC uses
  // one outstanding event and an ACK to handle coalescing on the browser
  // side. We should be able to clobber them in the main thread event queue.
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget, const gfx::Point& extent) {
        if (!widget)
          return;

        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;

        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        focused_frame->MoveRangeSelectionExtent(
            widget->ConvertWindowPointToViewport(extent));
      },
      widget_, extent));
}

void FrameInputHandlerImpl::ScrollFocusedEditableNodeIntoRect(
    const gfx::Rect& rect) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget, const gfx::Rect& rect) {
        if (!widget)
          return;

        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;

        RenderFrameImpl* render_frame =
            RenderFrameImpl::FromWebFrame(focused_frame);

        if (!render_frame)
          return;

        // OnSynchronizeVisualProperties does not call DidChangeVisibleViewport
        // on OOPIFs. Since we are starting a new scroll operation now, call
        // DidChangeVisibleViewport to ensure that we don't assume the element
        // is already in view and ignore the scroll.
        render_frame->ResetHasScrolledFocusedEditableIntoView();
        render_frame->ScrollFocusedEditableElementIntoRect(rect);
      },
      widget_, rect));
}

void FrameInputHandlerImpl::MoveCaret(const gfx::Point& point) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<RenderWidget> widget, const gfx::Point& point) {
        if (!widget)
          return;

        auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
        if (!focused_frame)
          return;
        focused_frame->MoveCaretSelection(
            widget->ConvertWindowPointToViewport(point));
      },
      widget_, point));
}

void FrameInputHandlerImpl::ExecuteCommandOnMainThread(
    base::WeakPtr<RenderWidget> widget,
    const std::string& command,
    UpdateState update_state) {
  if (!widget)
    return;

  HandlingState handling_state(widget, update_state);
  auto* focused_frame = widget->GetFocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->ExecuteCommand(blink::WebString::FromUTF8(command));
}

FrameInputHandlerImpl::HandlingState::HandlingState(
    const base::WeakPtr<RenderWidget>& render_widget,
    UpdateState state)
    : render_widget_(render_widget),
      original_select_range_value_(render_widget->handling_select_range()),
      original_pasting_value_(render_widget->is_pasting()) {
  switch (state) {
    case UpdateState::kIsPasting:
      render_widget->set_is_pasting(true);
      FALLTHROUGH;  // Matches RenderFrameImpl::OnPaste() which sets both.
    case UpdateState::kIsSelectingRange:
      render_widget->set_handling_select_range(true);
      break;
    case UpdateState::kNone:
      break;
  }
}

FrameInputHandlerImpl::HandlingState::~HandlingState() {
  // RenderFrame may have been destroyed while this object was on the stack.
  if (!render_widget_)
    return;
  render_widget_->set_handling_select_range(original_select_range_value_);
  render_widget_->set_is_pasting(original_pasting_value_);
}

}  // namespace content
