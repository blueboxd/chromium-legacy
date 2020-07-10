# `Source/core/animation`

This directory contains the main thread animation engine. This implements the
Web Animations timing model that drives CSS Animations, Transitions and exposes
the Web Animations API (e.g. `element.animate()`) to Javascript.

## Contacts

* [Animations OWNERS](
https://cs.chromium.org/search?&q=file:blink/renderer/core/animation/OWNERS)
* [Chromium #animations channel on Slack](
https://chromium.slack.com#animations)

## Specifications implemented

*   [CSS Animations Level 1](https://drafts.csswg.org/css-animations-1/)
*   [CSS Animations Level 2](https://drafts.csswg.org/css-animations-2/)
*   [CSS Transitions Level 1](https://drafts.csswg.org/css-transitions-1/)
*   [CSS Transitions Level 2](https://drafts.csswg.org/css-transitions-2/)
*   [Web Animations Level 1](https://drafts.csswg.org/web-animations-1/)
*   [CSS Properties and Values API Level 1 - Animation Behavior of Custom Properties](
https://www.w3.org/TR/css-properties-values-api-1/#animation-behavior-of-custom-properties)
*   Individual CSS property animation behaviour [e.g. transform interolation](
https://www.w3.org/TR/css-transforms-1/#interpolation-of-transforms).

## Types of animations

There are 3 main types of animations that are supported in this directory
subtree: CSS animations, CSS transitions, and web animations. These animation
types are largely differentiated by their creation mechanism, but also have
differences in behavior such as rules for composite ordering. Nonetheless, all
three animation types share a common base class ([Animation](
https://cs.chromium.org/search/?q=class:blink::Animation$)) with the majority
of the code being common to all three animation types.

### CSS animations

CSS animations are created via CSS rules, whereby an animation-name property
(or animation shorthand property) refers to one or more keyframes rules.
Keyframes specify property values at various positions along the animation
curve. For example:

```css
@keyframes fade {
  0% { opacity: 1; }
  100% { opacity: 0; }
}
@keyframes redshift {
  to { color: red; }
}
.fade-out {
  animation: fade 1s linear, redshift 400ms ease-in-out;
}
```

The labels 'from' and 'to' are aliases for '0%' and '100%', respectively.
Keyframes may specify different property values, and the set of keyframes is not
required to specify values at 0% or 100% (partial keyframes).  If missing
starting or ending values for a property, a neutral property values is used
based on the underlying value. In the above example, the redshift animation will
animate from the current color to red.

This example illustrates just a few of the options for generating keyframes.
A more detailed explanation can be found on the [MDN @keyframes](
https://developer.mozilla.org/en-US/docs/Web/CSS/@keyframes) page. A more
detailed explanation of the animation shorthand property and its longhand
counterparts can be found on the [MDN > CSS > animation](
https://developer.mozilla.org/en-US/docs/Web/CSS/animation) page.

[CSS Animations](https://cs.chromium.org/search?&q=class:blink::CSSAnimation$)
are created during style update in a multi-stage process. Keyframe models for
pending animations are created in [CalculateAnimationUpdate](
https://cs.chromium.org/search?lang=cc&q=function:CSSAnimations::CalculateAnimationUpdate
), which in turn calls [CreateKeyframeEffectModel](
https://cs.chromium.org/search?lang=cc&q=function:CreateKeyframeEffectModel%20file:css_animations.cc
). The algorithm for constructing keyframes for a CSS animation is outlined in
[css-animations-2/#keyframes](https://drafts.csswg.org/css-animations-2/#keyframes).
New animations are constructed for the pending animations in
[MaybeApplyPendingUpdate](
https://cs.chromium.org/search?lang=cc&q=function:CSSAnimations::MaybeApplyPendingUpdate
). This method also handles updating the timing model of existing CSS animations
or canceling CSS animations.

Changes to the [phase](
https://www.w3.org/TR/web-animations-1/#animation-effect-phases-and-states)
 of a CSS animation may result in firing one or more [AnimationEvent](
https://developer.mozilla.org/en-US/docs/Web/API/AnimationEvent)s.  Each CSS
animation has an associated [AnimationEventDelegate](
https://cs.chromium.org/search?lang=cc&q=class:AnimationEventDelegate) with an
[OnEventCondition](
https://cs.chromium.org/search?lang=cc&q=function:AnimationEventDelegate::OnEventCondition) method,
which handles dispatching of these events.

The [CSSAnimation interface](
https://cs.chromium.org/search?q=file:css_animation.idl) extends the
[Animation interface](https://cs.chromium.org/search?q=file:animation.idl) to
include an animationName property, which indicated the name of the keyframes
rule associated with the animation.  Note that this association may be broken
by interacting with the animation via the web-animation API.


### CSS transition

Like CSS animations, CSS transitions are triggered by style rules in CSS;
however, unlike CSS animations, the keyframes are inferred. For example:

```css
button {
  background: blue;
  transition: background-color 150ms ease-in;
}

button:hover {
  background: #05f
}
```

In this example, hovering over a button changes the background color. A
transition animation is created from the previous background color to the new
background color. A more detailed description of the transition property and its
longhand counterparts can be found on the [MDN > CSS > Transitions](
https://developer.mozilla.org/en-US/docs/Web/CSS/transition) page.

[CSS Transitions](https://cs.chromium.org/search?&q=class:blink::CSSTransition$)
are created during style update in a multi-stage process. Keyframe models for
pending animations are created in [CalculateTransitionUpdate](
https://cs.chromium.org/search?lang=cc&q=function:CSSAnimations::CalculateTransitionUpdate$), which
iterates over names in transition-property calling
[CalculateTransitionUpdateForProperty](https://cs.chromium.org/search?lang=cc&q=function:CSSAnimations::CalculateTransitionUpdateForProperty)
for each property. If there is already an active transition for the property
and the value of the property changes, the transition is retargeted to the new
end point.  When retargeting an animation, the current position is used as a
starting point, which is calculated by applying active animations with an
updated timestamp to the underlying style in [CalculateBeforeChangeStyle](
https://cs.chromium.org/search?lang=cc&q=function:CSSAnimations::CalculateBeforeChangeStyle
). The before-change style is lazy evaluated to avoid unnecessary work when no
transition updates are required, and is computed at most once per element per
non-animation style update. New transitions are constructed for the pending
transitions in [MaybeApplyPendingUpdate](
https://cs.chromium.org/search?lang=cc&q=function:CSSAnimations::MaybeApplyPendingUpdate
). This method handles constructing new transitions, retargeting active
transitions, and canceling finished transitions.

Changes to the [phase](
https://www.w3.org/TR/web-animations-1/#animation-effect-phases-and-states)
of a CSS transition may result in firing one or more [TransitionEvent](
https://developer.mozilla.org/en-US/docs/Web/API/TransitionEvent)s.  Each CSS
transition has an associated [TransitionEventDelegate](
https://cs.chromium.org/search?lang=cc&q=class:TransitionEventDelegate) with an
[OnEventCondition](
https://cs.chromium.org/search?lang=cc&q=function:TransitionEventDelegate::OnEventCondition) method,
which handles dispatching of these events.

The [CSSTransition interface](
https://cs.chromium.org/search?q=file:css_transition.idl) extends the
[Animation interface](https://cs.chromium.org/search?q=file:animation.idl) to
include an transitionProperty property, which indicated the name of the property
being transitioned. Note that this association may be broken by interacting with
the transition via the web-animation API.

### Web Animation

Web animations are programmatically created in JavaScript via calls to
[Element.animate](
https://cs.chromium.org/search?q=function:Animatable::Animate). Similar to
CSS animations, web-animations require a set of keyframes; however, the
representation of the keyframes is expressed as a list or object in JavaScript.
For example:

```javascript
const slideAnimation = element.animate(
  [ { offset: 1, transform: 'translateX(200px)', easing: 'linear' } ],
  { duration: 1000,  easing: linear });
const gravityAnimation = element.animate(
  { transform: ['none', 'translateY(400px' ] },
  { duration: 1000, easing: 'ease-in', composite: 'add' });
```

This example illustrates two formats for expressing the list of keyframes.
The keyframes argument to animate may be a list of keyframes with optional
offsets. If offsets are not specified, then equal spacing is assumed. The first
of the two animations illustrates the use of partial keyframes. For this
animation, a neutral keyframe is inferred at offset 0.

If animating a single property, they keyframes may be expressed as a object with
the property name as a key and a list of string/numeric values for the property.
In this case, equally spaced keyframes are implied. This example also
illustrates the use of composite modes to combine multiple effects on a single
property. Since the easing functions are different for the two animations, they
cannot be combined into a single animation to achieve the desired results.

A more detailed description of the animate method can be found on the
[MDN Element.animate](
https://developer.mozilla.org/en-US/docs/Web/API/Element/animate) page.


## Integration with Chromium

The Blink animation engine interacts with Blink/Chrome in the following ways:

*   ### [Blink's Style engine](../css)

    The most user visible functionality of the animation engine is animating
    CSS values. This means animations have a place in the [CSS cascade][] and
    influence the [ComputedStyle][]s returned by [styleForElement()][].

    The implementation for this lives under [ApplyAnimatedStandardProperties()][]
    for standard properties and [ApplyAnimatedCustomProperties()][] for custom
    properties. Custom properties have more complex application logic due to
    dynamic dependencies introduced by [variable references].

    Animations can be controlled by CSS via the [`animation`](https://www.w3.org/TR/css-animations-1/#animation)
    and [`transition`](https://www.w3.org/TR/css-transitions-1/#transition-shorthand-property) properties.
    In code this happens when [styleForElement()][] uses [CalculateAnimationUpdate()][]
    and [CalculateTransitionUpdate()][] to build a [set of mutations][] to make
    to the animation engine which gets [applied later][].

[CSS cascade]: https://www.w3.org/TR/css-cascade-3/#cascade-origin
[ComputedStyle]: https://cs.chromium.org/search/?q=class:blink::ComputedStyle$
[styleForElement()]: https://cs.chromium.org/search/?q=function:StyleResolver::styleForElement
[ApplyAnimatedStandardProperties()]: https://cs.chromium.org/?type=cs&q=function:StyleResolver::ApplyAnimatedStandardProperties
[ApplyAnimatedCustomProperties()]: https://cs.chromium.org/?type=cs&q=function:ApplyAnimatedCustomProperties
[variable references]: https://www.w3.org/TR/css-variables-1/#using-variables
[CalculateAnimationUpdate()]: https://cs.chromium.org/search/?q=function:CSSAnimations::CalculateAnimationUpdate
[CalculateTransitionUpdate()]: https://cs.chromium.org/search/?q=function:CSSAnimations::CalculateTransitionUpdate
[MaybeApplyPendingUpdate()]: https://cs.chromium.org/search/?q=function:CSSAnimations::MaybeApplyPendingUpdate
[set of mutations]: https://cs.chromium.org/search/?q=class:CSSAnimationUpdate
[applied later]: https://cs.chromium.org/search/?q=function:Element::StyleForLayoutObject+MaybeApplyPendingUpdate

*   ### [Chromium's Compositor](../../../../../cc/README.md)

    Chromium's compositor has a separate, more lightweight [animation
    engine](../../../../../cc/animation/README.md) that runs separate to the
    main thread. Blink's animation engine delegates animations to the compositor
    where possible for better performance and power utilisation.

    #### Compositable animations

    A subset of style properties (currently transform, opacity, filter, and
    backdrop-filter) can be mutated on the compositor thread. Animations that
    mutate only these properties are candidates for being accelerated and run
    on the compositor thread which ensures they are isolated from Blink's main
    thread work.

    Whether or not an animation can be accelerated is determined by
    [CheckCanStartAnimationOnCompositor()][] which looks at several aspects
    such as the composite mode, other animations affecting same property, and
    whether the target element can be promoted and mutated in compositor.
    Reasons for not compositing animations are captured in [FailureCodes][].

    #### Lifetime of a compositor animation

    Animations that can be accelerated get added to the [PendingAnimations][]
    list. The pending list is updated as part of document lifecycle and ensures
    each pending animation gets a corresponding [cc::AnimationPlayer][]
    representing the animation on the compositor. The player is initialized with
    appropriate timing values and corresponding effects.

    Note that changing that animation playback rate, start time, or effect,
    simply adds the animation back on to the pending list and causes the
    compositor animation to be cancelled and a new one to be started. See
    [Animation::PreCommit()][] for more details.

    An accelerated animation is still running on main thread ensuring that its
    effective output is reflected in the Element style. So while the compositor
    animation updates the visuals the main thread animation updates the computed
    style. There is a special case logic to ensure updates from such accelerated
    animations do not cause spurious commits from main to compositor (See
    [CompositedLayerMapping::UpdateGraphicsLayerGeometry()][], or
    [FragmentPaintPropertyTreeBuilder::UpdateTransform()][],
    [FragmentPaintPropertyTreeBuilder::UpdateEffect()][], and
    [FragmentPaintPropertyTreeBuilder::UpdateFilter()][] for
    [BlinkGenPropertyTrees mode][])

    A compositor animation provides updates on its playback state changes (e.g.,
    on start, finish, abort) to its blink counterpart via
    [CompositorAnimationDelegate][] interface. Blink animation uses the start
    event callback to obtain an accurate start time for the animation which is
    important to ensure its output accurately reflects the compositor animation
    output.

[CheckCanStartAnimationOnCompositor()]: https://cs.chromium.org/search/?q=file:animation.h+function:CheckCanStartAnimationOnCompositor
[FailureCodes]: https://cs.chromium.org/search/?q=return%5Cs%2B(CompositorAnimations::)?FailureCode
[cc::AnimationPlayer]: https://cs.chromium.org/search/?q=file:src/cc/animation/animation_player.h+class:AnimationPlayer
[PendingAnimations]: https://cs.chromium.org/search/?q=file:pending_animations.h+class:PendingAnimations
[Animation::PreCommit()]: https://cs.chromium.org/search/?q=file:animation.h+function:PreCommit
[CompositorAnimationDelegate]: https://cs.chromium.org/search/?q=file:compositor_animation_delegate.h
[CompositedLayerMapping::UpdateGraphicsLayerGeometry()]: https://cs.chromium.org/search/?q=file:composited_layer_mapping.h+function:UpdateGraphicsLayerGeometry
[FragmentPaintPropertyTreeBuilder::UpdateTransform()]: https://cs.chromium.org/search/?q=class:FragmentPaintPropertyTreeBuilder+function:UpdateTransform
[FragmentPaintPropertyTreeBuilder::UpdateEffect()]: https://cs.chromium.org/search/?q=class:FragmentPaintPropertyTreeBuilder+function:UpdateEffect
[FragmentPaintPropertyTreeBuilder::UpdateFilter()]: https://cs.chromium.org/search/?q=class:FragmentPaintPropertyTreeBuilder+function:UpdateFilter
[BlinkGenPropertyTrees mode]: https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/paint/README.md

*   ### Javascript

    [EffectInput](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/animation/effect_input.cc)
    contains the helper functions that are used to
    [process a keyframe argument](https://drafts.csswg.org/web-animations/#processing-a-keyframes-argument)
    which can take an argument of either object or array form.

*   ### DevTools

    The animations timeline uses [InspectorAnimationAgent][] to track all active
    animations. This class has interfaces for pausing, adjusting
    DocumentTimeline playback rate, and seeking animations.

    InspectorAnimationAgent clones the inspected animation in order to avoid
    firing animation events, and suppresses the effects of the original
    animation. From this point on, modifications can be made to the cloned
    animation without having any effect on the underlying animation or its
    listeners.

[InspectorAnimationAgent]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/inspector/InspectorAnimationAgent.h

*   ### SVG

    The `element.animate()` API supports targeting SVG attributes in its
    keyframes. This is an experimental implementation guarded by the
    WebAnimationsSVG flag and not exposed on the web.

    This feature should provide a high fidelity alternative to our SMIL
    implementation.

## Architecture

### Animation Timing Model

The animation engine is built around the
[timing model](https://www.w3.org/TR/web-animations-1/#timing-model) described
in the Web Animations spec.

This describes a hierarchy of entities:

*   __[DocumentTimeline][]__: Represents the wall clock time.
    *   __[Animation][]__: Represents an individual animation and when it
        started playing.
        *   __[AnimationEffect][]__: Represents the effect an animation has
            during the animation (e.g. updating an element's color property).

Time trickles down from the [DocumentTimeline][] and is transformed at each
stage to produce some progress fraction that can be used to apply the effects of
the animations.

For example:

```javascript
// Page was loaded at 2:00:00PM, the time is currently 2:00:10PM.
// document.timeline.currentTime is currently 10000 (10 seconds).

let animation = element.animate([
    {transform: 'none'},
    {transform: 'rotate(200deg)'},
  ], {
    duration: 20000,  // 20 seconds
  });

animation.startTime = 6000;  // 6 seconds
```

*   __[DocumentTimeline][]__ notifies that the time is 10 seconds.
    *   __[Animation][]__ computes that its currentTime is 4 seconds due to its
        startTime being at 6 seconds.
        *   __[AnimationEffect][]__ has a duration of 20 seconds and computes
            that it has a progress of 20% from the parent animation being 4
            seconds into the animation.

            The effect is animating an element from `transform: none` to
            `transform: rotate(200deg)` so it computes the current effect to be
            `transfrom: rotate(40deg)`.

[Animation]: https://cs.chromium.org/search/?q=class:blink::Animation$
[AnimationEffect]: https://cs.chromium.org/search/?q=class:blink::AnimationEffect$
[DocumentTimeline]: https://cs.chromium.org/search/?q=class:blink::DocumentTimeline$
[EffectStack]: https://cs.chromium.org/search/?q=class:blink::EffectStack

### Lifecycle of an Animation

![Lifecycle]

1.  An [Animation][] is created via CSS<sup>1</sup> or `element.animate()`.
2.  At the start of the next frame the [Animation][] and its [AnimationEffect][]
    are updated with the currentTime of the [DocumentTimeline][].
3.  The [AnimationEffect][] gets sampled with its computed localTime, pushes a
    [SampledEffect][] into its target element's [EffectStack][] and marks the
    elements style as dirty to ensure it gets updated later in the document
    lifecycle.
4.  During the next [style resolve][styleForElement()] on the target element all
    the [SampledEffect][]s in its [EffectStack][] are incorporated into building
    the element's [ComputedStyle][].

One key takeaway here is to note that timing updates are done in a separate
phase to effect application. Effect application must occur during style
resolution which is a highly complex process with a well defined place in the
document lifecycle. Updates to animation timing will request style updates
rather than invoke them directly.

<sup>1</sup> CSS animations and transitions are actually created/destroyed
during style resolve (step 4). There is special logic for forcing these
animations to have their timing updated and their effects included in
the same style resolve. An unfortunate side effect of this is that style
resolution can cause style to get dirtied, this is currently a
[code health bug](http://crbug.com/492887).

[Lifecycle]: images/lifecycle.png
[SampledEffect]: https://cs.chromium.org/search/?q=class:blink::SampledEffect

### [KeyframeEffect][]

Currently all animations use [KeyframeEffect][] for their [AnimationEffect][].
The generic [AnimationEffect][] from which it inherits is an extention point in
Web Animations where other kinds of animation effects can be defined later by
other specs (for example Javascript callback based effects).

#### Structure of a [KeyframeEffect][]

*   __[KeyframeEffect][]__ represents the effect an animation has (without any
    details of when it started or whether it's playing) and is comprised of
    three things:
    *   Some __[Timing][]__ information (inherited from [AnimationEffect][]).
        [Example](http://jsbin.com/nuyohulojo/edit?js,output):
        ```javascript
        {
          duration: 4000,
          easing: 'ease-in-out',
          iterations: 8,
          direction: 'alternate',
        }
        ```
        This is used to [compute][UpdateInheritedTime()] the percentage progress
        of the effect given the duration of time that the animation has been
        playing for.

    *   The DOM __[Element][]__ that is being animated.

    *   A __[KeyframeEffectModel][]__ that holds a sequence of keyframes to
        specify the properties being animated and what values they pass
        through.
        [Example](http://jsbin.com/wiyefaxiru/1/edit?js,output):
        ```javascript
        [
          {backgroundColor: 'red', transform: 'rotate(0deg)'},
          {backgroundColor: 'yellow'},
          {backgroundColor: 'lime'},
          {backgroundColor: 'blue'},
          {backgroundColor: 'red', transform: 'rotate(360deg)'},
        ]
        ```

        These keyframes are used to compute:
        *   A __[PropertySpecificKeyframe map][KeyframeGroupMap]__ that simply
            breaks up the input multi-property keyframes into per-property
            keyframe lists.
        *   An __[InterpolationEffect][]__ which holds a set of
            [Interpolation][]s, each one representing the animated values
            between adjacent pairs of [PropertySpecificKeyframe][]s, and where
            in the percentage progress they are active.
            In the example keyframes above the [Interpolations][] generated
            would include, among the 5 different property specific keyframe
            pairs, one for `backgroundColor: 'red'` to
            `backgroundColor: 'yellow'` that applied from 0% to 25% and one for
            `transform: 'rotate(0deg)'` to `transform: 'rotate(360deg)'` that
            applied from 0% to 100%.

[Element]: https://cs.chromium.org/search/?q=class:blink::Element$
[KeyframeGroupMap]: https://cs.chromium.org/search/?q=class:blink::KeyframeEffectModelBase+KeyframeGroupMap
[PropertySpecificKeyframe]: https://cs.chromium.org/search/?q=class:blink::Keyframe::PropertySpecificKeyframe
[KeyframeEffect]: https://cs.chromium.org/search/?q=class:blink::KeyframeEffect$
[KeyframeEffectModel]: https://cs.chromium.org/search/?q=class:blink::KeyframeEffectModelBase$
[Timing]: https://cs.chromium.org/search/?q=class:blink::Timing$
[UpdateInheritedTime()]: https://cs.chromium.org/search/?q=function:%5CbAnimationEffect::UpdateInheritedTime

#### Lifecycle of an [Interpolation][]

[Interpolation][] is the data structure that [style
resolution][styleForElement()] uses to resolve what animated value to apply
to an animated element's [ComputedStyle][].

1.   [Interpolation][]s are lazily
     [instantiated][EnsureInterpolationEffectPopulated()] prior to sampling.
2.   [KeyframeEffectModel][]s are [sampled][Sample()] every frame (or as
     necessary) for a stack of [Interpolation][]s to
     [apply][ApplyAnimatedStandardProperties()] to the associated [Element][]
     and stashed away in the [Element][]'s [ElementAnimations][]'
     [EffectStack][]'s [SampledEffect][]s.
3.   During [style resolution][styleForElement()] on the target [Element][] all
     the [Interpolation][]s are [collected and organised by
     category][AdoptActiveInterpolations] according to whether it's a transition
     or not (transitions in Blink are
     [suppressed][CalculateTransitionUpdateForProperty()] in the presence of
     non-transition animations on the same property) and whether it affects
     custom properties or not (animated custom properties are
     [animation-tainted](https://www.w3.org/TR/css-variables-1/#animation-tainted)
     and affect the [processing of animation
     properties][animation-tainted-processing].
4.   TODO(alancutter): Describe what happens in processing a stack of
     interpolations.

[AdoptActiveInterpolations]: https://cs.chromium.org/search/?q=AdoptActiveInterpolations%5Cw%2B
[animation-tainted-processing]: https://cs.chromium.org/search/?q=function:blink::StyleBuilder::ApplyProperty+animation_tainted
[CalculateTransitionUpdateForProperty()]: https://cs.chromium.org/search/?q=function:blink::CSSAnimations::CalculateTransitionUpdateForProperty
[ElementAnimations]: https://cs.chromium.org/search/?q=class:blink::ElementAnimations
[EnsureInterpolationEffectPopulated()]: https://cs.chromium.org/search/?q=function:KeyframeEffectModelBase::EnsureInterpolationEffectPopulated
[Interpolation]: https://cs.chromium.org/search/?q=class:blink::Interpolation$
[InterpolationEffect]: https://cs.chromium.org/search/?q=class:blink::InterpolationEffect
[Sample()]: https://cs.chromium.org/search/?q=function:KeyframeEffectModelBase::Sample

## Testing pointers

Test new animation features using end to end web-platform-tests to ensure
cross-browser interoperability. Use unit testing when access to chrome internals
is required. Test chrome specific features such as compositing of animation
using web tests or unit tests.

### End to end testing

Features in the Web Animations spec are tested in
[web-animations][web-animations-tests]. [Writing web platform tests][] has
pointers for how to get started. If Chrome does not correctly implement the
spec, add a corresponding -expected.txt file with your test listing the expected
failure in Chrome.

[Web tests](../../../../../docs/testing/writing_web_tests.md) are located
in [third_party/blink/web_tests][]. These should be written when needing end
to end testing but either when testing chrome specific features (i.e.
non-standardized) such as compositing or when the test requires access to chrome
internal features not easily tested by web-platform-tests.

[web-animations-tests]: https://cs.chromium.org/chromium/src/third_party/blink/web_tests/external/wpt/web-animations/
[Writing web platform tests]: ../../../../../docs/testing/web_platform_tests.md#Writing-tests
[third_party/blink/web_tests]: https://cs.chromium.org/chromium/src/third_party/blink/web_tests/animations/

### Unit testing

Unit testing of animations can range from [extending Test][] when you will
manually construct an instance of your object to [extending RenderingTest][]
where you can load HTML, [enable compositing][] if necessary, and run assertions
about the state.

[extending Test]: https://cs.chromium.org/search/?q=public%5C+testing::Test+file:core%5C/animation&sq=package:chromium&type=cs
[extending RenderingTest]: https://cs.chromium.org/search/?q=public%5C+RenderingTest+file:core%5C/animation&type=cs
[enable compositing]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/animation/compositor_animations_test.cc?type=cs&sq=package:chromium&q=file:core%5C/animation%5C/.*test%5C.cpp+EnableCompositing

## Ongoing work

### Properties And Values API

TODO: Summarize properties and values API.

### Web Animations API

TODO: Summarize Web Animations API.

### [Animation Worklet](../../modules/animationworklet/README.md)

AnimationWorklet is a new primitive for creating high performance procedural
animations on the web. It is being incubated as part of the
[CSS Houdini task force](https://github.com/w3c/css-houdini-drafts/wiki), and if
successful will be transferred to that task force for full standardization.

A [WorkletAnimation][] behaves and exposes the same animation interface as other
web animation but it allows the animation itself to be highly customized in
Javascript by providing an `animate` callback. These animations run inside an
isolated worklet global scope.

[WorkletAnimation]: https://cs.chromium.org/search/?q=file:animationworklet/worklet_animation.h+class:WorkletAnimation
