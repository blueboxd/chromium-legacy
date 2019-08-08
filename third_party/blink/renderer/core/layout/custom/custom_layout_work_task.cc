// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_layout_work_task.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_child.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_constraints_options.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment.h"
#include "third_party/blink/renderer/core/layout/custom/layout_custom.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

CustomLayoutWorkTask::CustomLayoutWorkTask(
    CustomLayoutChild* child,
    CustomLayoutToken* token,
    ScriptPromiseResolver* resolver,
    const CustomLayoutConstraintsOptions* options,
    scoped_refptr<SerializedScriptValue> constraint_data)
    : child_(child),
      token_(token),
      resolver_(resolver),
      options_(options),
      constraint_data_(std::move(constraint_data)) {}

CustomLayoutWorkTask::~CustomLayoutWorkTask() = default;

void CustomLayoutWorkTask::Run() {
  DCHECK(token_->IsValid());

  LayoutBox* box = child_->GetLayoutBox();
  const ComputedStyle& style = box->StyleRef();

  DCHECK(box->Parent());
  DCHECK(box->Parent()->IsLayoutCustom());
  DCHECK(box->Parent() == box->ContainingBlock());

  bool is_parallel_writing_mode = IsParallelWritingMode(
      box->Parent()->StyleRef().GetWritingMode(), style.GetWritingMode());

  if (options_->hasFixedInlineSize()) {
    if (is_parallel_writing_mode) {
      box->SetOverrideLogicalWidth(
          LayoutUnit::FromDoubleRound(options_->fixedInlineSize()));
    } else {
      box->SetOverrideLogicalHeight(
          LayoutUnit::FromDoubleRound(options_->fixedInlineSize()));
    }
  } else {
    box->SetOverrideContainingBlockContentLogicalWidth(
        options_->hasAvailableInlineSize() &&
                options_->availableInlineSize() >= 0.0
            ? LayoutUnit::FromDoubleRound(options_->availableInlineSize())
            : LayoutUnit());
  }

  if (options_->hasFixedBlockSize()) {
    if (is_parallel_writing_mode) {
      box->SetOverrideLogicalHeight(
          LayoutUnit::FromDoubleRound(options_->fixedBlockSize()));
    } else {
      box->SetOverrideLogicalWidth(
          LayoutUnit::FromDoubleRound(options_->fixedBlockSize()));
    }
  } else {
    box->SetOverrideContainingBlockContentLogicalHeight(
        options_->hasAvailableBlockSize() &&
                options_->availableBlockSize() >= 0.0
            ? LayoutUnit::FromDoubleRound(options_->availableBlockSize())
            : LayoutUnit());
  }

  // We default the percentage resolution block-size to indefinite if nothing
  // is specified.
  LayoutUnit percentage_resolution_logical_height(-1);

  if (is_parallel_writing_mode) {
    if (options_->hasPercentageBlockSize() &&
        options_->percentageBlockSize() >= 0.0) {
      percentage_resolution_logical_height =
          LayoutUnit::FromDoubleRound(options_->percentageBlockSize());
    } else if (options_->hasAvailableBlockSize() &&
               options_->availableBlockSize() >= 0.0) {
      percentage_resolution_logical_height =
          LayoutUnit::FromDoubleRound(options_->availableBlockSize());
    }
  } else {
    if (options_->hasPercentageInlineSize() &&
        options_->percentageInlineSize() >= 0.0) {
      percentage_resolution_logical_height =
          LayoutUnit::FromDoubleRound(options_->percentageInlineSize());
    } else if (options_->hasAvailableInlineSize() &&
               options_->availableInlineSize() >= 0.0) {
      percentage_resolution_logical_height =
          LayoutUnit::FromDoubleRound(options_->availableInlineSize());
    }
  }

  box->SetOverridePercentageResolutionBlockSize(
      percentage_resolution_logical_height);

  auto* layout_custom = DynamicTo<LayoutCustom>(box);
  if (layout_custom)
    layout_custom->SetConstraintData(constraint_data_);
  // TODO(cbiesinger): Can this just be ForceLayout()?
  box->ForceLayoutWithPaintInvalidation();

  box->ClearOverrideContainingBlockContentSize();
  box->ClearOverridePercentageResolutionBlockSize();
  box->ClearOverrideSize();

  if (layout_custom)
    layout_custom->ClearConstraintData();

  LayoutUnit fragment_inline_size =
      is_parallel_writing_mode ? box->LogicalWidth() : box->LogicalHeight();
  LayoutUnit fragment_block_size =
      is_parallel_writing_mode ? box->LogicalHeight() : box->LogicalWidth();

  resolver_->Resolve(MakeGarbageCollected<CustomLayoutFragment>(
      child_, token_, fragment_inline_size, fragment_block_size,
      resolver_->GetScriptState()->GetIsolate()));
}

}  // namespace blink
