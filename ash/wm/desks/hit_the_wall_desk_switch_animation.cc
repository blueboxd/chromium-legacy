// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/hit_the_wall_desk_switch_animation.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform.h"

namespace ash {

void PerformHitTheWallDeskSwitchAnimation(aura::Window* root, bool going_left) {
  DCHECK(root->IsRootWindow());

  // Start and end the animation using the root layer's target transform, since
  // the layer might have a different transform other than identity due to, for
  // example, display rotation.
  ui::Layer* layer = root->layer();
  const gfx::Transform end_transform = layer->GetTargetTransform();
  gfx::Transform begin_transform = end_transform;
  // |root| will be translated out horizontally by 4% of its width and then
  // translated back to its original transform.
  const float displacement_factor = 0.04f * (going_left ? 1 : -1);
  begin_transform.Translate(displacement_factor * root->bounds().width(), 0);

  // Prepare two animation elements, one for the outgoing translation:
  //      |     |
  //      |<----|
  //      |     |
  // and another for the incoming translation:
  //      |     |
  //      |---->|
  //      |     |
  constexpr base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(150);
  auto outgoing_transition = ui::LayerAnimationElement::CreateTransformElement(
      begin_transform, kDuration);
  outgoing_transition->set_tween_type(gfx::Tween::EASE_OUT);
  auto sequence = std::make_unique<ui::LayerAnimationSequence>();
  sequence->AddElement(std::move(outgoing_transition));
  auto incoming_transition = ui::LayerAnimationElement::CreateTransformElement(
      end_transform, kDuration);
  incoming_transition->set_tween_type(gfx::Tween::EASE_IN);
  sequence->AddElement(std::move(incoming_transition));

  // Use `REPLACE_QUEUED_ANIMATIONS` since the user may press the shortcut many
  // times repeatedly, and we don't want to keep animating the root layer
  // endlessly.
  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  settings.SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  layer->GetAnimator()->StartAnimation(sequence.release());
}

}  // namespace ash
