/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/svg/svg_resources_cycle_solver.h"

namespace blink {

SVGResourcesCycleSolver::SVGResourcesCycleSolver() = default;

SVGResourcesCycleSolver::~SVGResourcesCycleSolver() = default;

bool SVGResourcesCycleSolver::IsKnownAcyclic(
    const LayoutSVGResourceContainer* resource) const {
  return dag_cache_.Contains(resource);
}

void SVGResourcesCycleSolver::AddAcyclicSubgraph(
    const LayoutSVGResourceContainer* resource) {
  dag_cache_.insert(resource);
}

bool SVGResourcesCycleSolver::EnterResource(
    const LayoutSVGResourceContainer* resource) {
  return active_resources_.insert(resource).is_new_entry;
}

void SVGResourcesCycleSolver::LeaveResource(
    const LayoutSVGResourceContainer* resource) {
  active_resources_.erase(resource);
}

}  // namespace blink
