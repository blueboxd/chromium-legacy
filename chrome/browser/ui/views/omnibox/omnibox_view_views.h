// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_sub_menu_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/range/range.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/chromeos/input_method_manager.h"
#endif

class LocationBarView;
class OmniboxClient;
class OmniboxPopupContentsView;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class RenderText;
}  // namespace gfx

namespace ui {
class OSExchangeData;
}  // namespace ui

// Views-implementation of OmniboxView.
class OmniboxViewViews : public OmniboxView,
                         public views::Textfield,
#if defined(OS_CHROMEOS)
                         public chromeos::input_method::InputMethodManager::
                             CandidateWindowObserver,
#endif
                         public views::TextfieldController,
                         public ui::CompositorObserver,
                         public TemplateURLServiceObserver,
                         public content::WebContentsObserver {
 public:
  // The internal view class name.
  static const char kViewClassName[];

  // Range of command IDs to use for the items in the send tab to self submenu.
  static const int kMinSendTabToSelfSubMenuCommandId =
      send_tab_to_self::SendTabToSelfSubMenuModel::kMinCommandId;
  static const int kMaxSendTabToSelfSubMenuCommandId =
      send_tab_to_self::SendTabToSelfSubMenuModel::kMaxCommandId;

  OmniboxViewViews(OmniboxEditController* controller,
                   std::unique_ptr<OmniboxClient> client,
                   bool popup_window_mode,
                   LocationBarView* location_bar,
                   const gfx::FontList& font_list);
  ~OmniboxViewViews() override;

  // Initialize, create the underlying views, etc.
  void Init();

  // Exposes the RenderText for tests.
#if defined(UNIT_TEST)
  gfx::RenderText* GetRenderText() {
    return views::Textfield::GetRenderText();
  }
#endif

  // For use when switching tabs, this saves the current state onto the tab so
  // that it can be restored during a later call to Update().
  void SaveStateToTab(content::WebContents* tab);

  // Called when the window's active tab changes.
  void OnTabChanged(content::WebContents* web_contents);

  // Called to clear the saved state for |web_contents|.
  void ResetTabState(content::WebContents* web_contents);

  // Installs the placeholder text with the name of the current default search
  // provider. For example, if Google is the default search provider, this shows
  // "Search Google or type a URL" when the Omnibox is empty and unfocused.
  void InstallPlaceholderText();

  // Indicates if the cursor is at one end of the input. Requires that both
  // ends of the selection reside there.
  bool SelectionAtBeginning() const;
  bool SelectionAtEnd() const;

  // Returns the width in pixels needed to display the current text. The
  // returned value includes margins.
  int GetTextWidth() const;
  // Returns the width in pixels needed to display the current text unelided.
  int GetUnelidedTextWidth() const;

  // Returns the omnibox's width in pixels.
  int GetWidth() const;

  // OmniboxView:
  void EmphasizeURLComponents() override;
  void Update() override;
  base::string16 GetText() const override;
  using OmniboxView::SetUserText;
  void SetUserText(const base::string16& text,
                   bool update_popup) override;
  void SetWindowTextAndCaretPos(const base::string16& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetAdditionalText(const base::string16& additional_text) override;
  void EnterKeywordModeForDefaultSearchProvider() override;
  bool IsSelectAll() const override;
  void GetSelectionBounds(base::string16::size_type* start,
                          base::string16::size_type* end) const override;
  size_t GetAllSelectionsLength() const override;
  void SelectAll(bool reversed) override;
  void RevertAll() override;
  void SetFocus(bool is_user_initiated) override;
  bool IsImeComposing() const override;
  gfx::NativeView GetRelativeWindowForPopup() const override;
  bool IsImeShowingPopup() const override;

  // views::Textfield:
  gfx::Size GetMinimumSize() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  ui::TextInputType GetTextInputType() const override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* navigation) override;
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;

  // For testing only.
  OmniboxPopupContentsView* GetPopupContentsViewForTesting() const {
    return popup_view_.get();
  }

 protected:
  // views::Textfield:
  void OnThemeChanged() override;
  bool IsDropCursorForInsertion() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsRevealOnHoverTest, HoverAndExit);
  FRIEND_TEST_ALL_PREFIXES(
      OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest,
      UserInteractionAndHover);
  FRIEND_TEST_ALL_PREFIXES(
      OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest,
      HideOnInteractionAfterFocusAndBlur);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsRevealOnHoverTest, AfterBlur);
  FRIEND_TEST_ALL_PREFIXES(
      OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest,
      PathChangeDuringAnimation);
  FRIEND_TEST_ALL_PREFIXES(
      OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest,
      VerticalAndHorizontalPosition);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsHideOnInteractionTest,
                           SameDocNavigations);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsHideOnInteractionTest,
                           SameDocNavigationDuringAnimation);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsHideOnInteractionTest,
                           UserInteractionDuringAnimation);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsHideOnInteractionTest,
                           SubframeNavigations);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsRevealOnHoverTest,
                           AlwaysShowFullURLs);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsHideOnInteractionTest,
                           AlwaysShowFullURLs);
  FRIEND_TEST_ALL_PREFIXES(
      OmniboxViewViewsRevealOnHoverAndMaybeHideOnInteractionTest,
      UnsetAlwaysShowFullURLs);
  // TODO(tommycli): Remove the rest of these friends after porting these
  // browser tests to unit tests.
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, CloseOmniboxPopupOnTextDrag);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, FriendlyAccessibleLabel);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, DoNotNavigateOnDrop);

  // Animates the URL to |elide_to_bounds|, which could be a substring or
  // superstring of what's currently displayed. The elision starts after
  // |delay_ms| ms. An elision animation hides the path (and optionally
  // subdomains) by narrowing the bounds of each side of the URL while also
  // shifting the text to remain aligned with the leading edge of the display
  // area. An unelision animation is the reverse.
  //
  // Animation is used for elision when the elision is in response to a user
  // interaction and we want to draw attention to where the URL is going and how
  // it can be retrieved. Depending on field trial configurations, this could be
  // after the user interacts with the page (where we want to hide the full URL
  // but hint that it can be brought back by interacting with the omnibox),
  // and/or when the user hovers over the omnibox. In contrast,
  // ElideToSimplifiedDomain() and UnelideFromSimplifiedDomain() instantly
  // elide/unelide and are used when we want to elide/unelide without drawing
  // the user's attention (for example, on a same-document navigation where we
  // want the URL to remain simplified if it was simplified before the
  // navigation).
  //
  // This class is declared here for testing.
  class ElideAnimation : public views::AnimationDelegateViews {
   public:
    ElideAnimation(OmniboxViewViews* view, gfx::RenderText* render_text);
    ~ElideAnimation() override;

    void Start(const gfx::Range& elide_to_bounds, uint32_t delay_ms);

    void Stop();

    // Returns true if the animation is currently running.
    bool IsAnimating();

    // Returns the bounds to which the animation is eliding, as passed in to
    // Start().
    const gfx::Range& GetElideToBounds() const;

    gfx::MultiAnimation* GetAnimationForTesting();

    // views::AnimationDelegateViews:
    void AnimationProgressed(const gfx::Animation* animation) override;
    void AnimationEnded(const gfx::Animation* animation) override;

   private:
    // Non-owning pointers. |view_| and |render_text_| must always outlive this
    // class.
    OmniboxViewViews* view_;
    gfx::RenderText* render_text_;

    // The target bounds passed in to Start().
    gfx::Range elide_to_bounds_;
    // The desired end state: the display rect that we are eliding or uneliding
    // to.
    gfx::Rect elide_to_rect_;
    // The starting display rect from which we are eliding or uneliding.
    gfx::Rect elide_from_rect_;
    // The starting and ending display offsets for |render_text_|.
    int starting_display_offset_ = 0;
    int ending_display_offset_ = 0;

    // The underlying animation. We use a MultiAnimation to implement the
    // |delay_ms| delay passed into Start(). When this delay is nonzero, the
    // first part of the animation is a zero tween of |delay_ms| length.
    std::unique_ptr<gfx::MultiAnimation> animation_;
  };

  enum class UnelisionGesture {
    HOME_KEY_PRESSED,
    MOUSE_RELEASE,
    OTHER,
  };

  // Update the field with |text| and set the selection. |ranges| should not be
  // empty; even text with no selections must have at least 1 empty range in
  // |ranges| to indicate the cursor position.
  void SetTextAndSelectedRanges(const base::string16& text,
                                const std::vector<gfx::Range>& ranges);

  void SetSelectedRanges(const std::vector<gfx::Range>& ranges);

  // Returns the selected text.
  base::string16 GetSelectedText() const;

  // Paste text from the clipboard into the omnibox.
  // Textfields implementation of Paste() pastes the contents of the clipboard
  // as is. We want to strip whitespace and other things (see GetClipboardText()
  // for details). The function invokes OnBefore/AfterPossibleChange() as
  // necessary.
  void OnOmniboxPaste();

  // Handle keyword hint tab-to-search and tabbing through dropdown results.
  bool HandleEarlyTabActions(const ui::KeyEvent& event);

  void ClearAccessibilityLabel();

  void SetAccessibilityLabel(const base::string16& display_text,
                             const AutocompleteMatch& match) override;

  // Returns true if the user text was updated with the full URL (without
  // steady-state elisions).  |gesture| is the user gesture causing unelision.
  bool UnapplySteadyStateElisions(UnelisionGesture gesture);

  // Informs if text and UI direction match (otherwise what "at end" means must
  // flip.)
  bool TextAndUIDirectionMatch() const;

  // Like SelectionAtEnd(), but accounts for RTL.
  bool DirectionAwareSelectionAtEnd() const;

  // If the Secondary button for the current suggestion is focused, clicks it
  // and returns true.
  bool MaybeTriggerSecondaryButton(const ui::KeyEvent& event);

#if defined(OS_MACOSX)
  void AnnounceFriendlySuggestionText();
#endif

  // OmniboxView:
  void SetCaretPos(size_t caret_pos) override;
  void UpdatePopup() override;
  void ApplyCaretVisibility() override;
  void OnTemporaryTextMaybeChanged(const base::string16& display_text,
                                   const AutocompleteMatch& match,
                                   bool save_original_selection,
                                   bool notify_text_changed) override;
  void OnInlineAutocompleteTextMaybeChanged(const base::string16& display_text,
                                            size_t user_text_start,
                                            size_t user_text_length) override;
  void OnInlineAutocompleteTextCleared() override;
  void OnRevertTemporaryText(const base::string16& display_text,
                             const AutocompleteMatch& match) override;
  void OnBeforePossibleChange() override;
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override;
  gfx::NativeView GetNativeView() const override;
  void ShowVirtualKeyboardIfEnabled() override;
  void HideImeIfNeeded() override;
  int GetOmniboxTextLength() const override;
  void SetEmphasis(bool emphasize, const gfx::Range& range) override;
  void UpdateSchemeStyle(const gfx::Range& range) override;

  // views::View
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::Textfield:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  const char* GetClassName() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void OnFocus() override;
  void OnBlur() override;
  base::string16 GetSelectionClipboardText() const override;
  void DoInsertChar(base::char16 ch) override;
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void ExecuteTextEditCommand(ui::TextEditCommand command) override;
  bool ShouldShowPlaceholderText() const override;

  // chromeos::input_method::InputMethodManager::CandidateWindowObserver:
#if defined(OS_CHROMEOS)
  void CandidateWindowOpened(
      chromeos::input_method::InputMethodManager* manager) override;
  void CandidateWindowClosed(
      chromeos::input_method::InputMethodManager* manager) override;
#endif

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void OnBeforeUserAction(views::Textfield* sender) override;
  void OnAfterUserAction(views::Textfield* sender) override;
  void OnAfterCutOrCopy(ui::ClipboardBuffer clipboard_buffer) override;
  void OnWriteDragData(ui::OSExchangeData* data) override;
  void OnGetDragOperationsForTextfield(int* drag_operations) override;
  void AppendDropFormats(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) override;
  int OnDrop(const ui::OSExchangeData& data) override;
  void UpdateContextMenu(ui::SimpleMenuModel* menu_contents) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int id) const override;

  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override;
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnCompositingEnded(ui::Compositor* compositor) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

  // Returns the gfx::Range of the simplified domain of the current URL, if
  // there is one. The simplified domain could be either the registrable domain
  // (if OmniboxFieldTrial::ElideToRegistrableDomain() is enabled) or the full
  // hostname.
  gfx::Range GetSimplifiedDomainBounds();

  // Returns true if the currently displayed URL is eligible for elision to a
  // simplified domain. This takes into account the omnibox's current state
  // (e.g. the URL shouldn't be elided if the user is currently editing it) as
  // well as properties of the current text (e.g. extension URLs or non-URLs
  // shouldn't be elided because they may not have simplified domains).
  //
  // This method does NOT take field trials into account or the "Always show
  // full URLs" option. Calling code should check field trial state and
  // model()->ShouldPreventElision() if applicable.
  bool IsURLEligibleForSimplifiedDomainEliding();

  // When certain field trials are enabled, the URL is shown on page load
  // and elided to a simplified domain when the user interacts with the page.
  // This method resets back to the on-page-load state. That is, it unhides the
  // URL (if currently hidden) and resets state so that the URL will show until
  // user interaction. This is used on navigation and blur, when the URL should
  // be shown but hidden on next user interaction.
  void ResetToHideOnInteraction();

  // Called when the "Always show full URLs" preference is toggled. Updates the
  // state to elide to a simplified domain on user interaction and/or reveal the
  // URL on hover, depending on field trial configuration.
  //
  // When the preference changes, we immediately elide/unelide instead of
  // animating. Animating might look a little nicer, but this should be a
  // relatively rare event so it's simpler to just immediately update the
  // display.
  void OnShouldPreventElisionChanged();

  // Elides or unelides to a simplified version of the URL. Callers should
  // ensure that the URL is valid before calling.
  //
  // These methods do not animate, but rather immediately elide/unelide. These
  // methods are used when we don't want to draw the user's attention to the URL
  // simplification -- for example, if the URL is already simplified and the
  // user performs a same-document navigation, we want to keep the URL
  // simplified without it appearing to be a change from the user's perspective.
  virtual void ElideToSimplifiedDomain();
  virtual void UnelideFromSimplifiedDomain();

  ElideAnimation* GetHoverElideOrUnelideAnimationForTesting();
  ElideAnimation* GetElideAfterInteractionAnimationForTesting();

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (smaller font size). This is used for popups.
  bool popup_window_mode_;

  std::unique_ptr<OmniboxPopupContentsView> popup_view_;

  // Animations are used to elide/unelide the path (and subdomains, if
  // OmniboxFieldTrial::ShouldElideToRegistrableDomain() is true) under some
  // field trial settings. These animations are created at different times
  // depending on the field trial configuration, so don't assume they are
  // non-null.
  //
  // These animations are used by different field trials as described below.

  // When ShouldRevealPathQueryRefOnHover() is enabled but not
  // ShouldHidePathQueryRefOnInteraction(), then the URL is elided in
  // EmphasizeUrlComponents() and |hover_elide_or_unelide_animation_| is created
  // in OnThemeChanged(). This animation is used to unelide or elide the URL
  // when the mouse hovers or exits the omnibox.
  std::unique_ptr<ElideAnimation> hover_elide_or_unelide_animation_;
  // When ShouldHidePathQueryRefOnInteraction() is enabled, we don't
  // create any animations until the user interacts with the page. When a
  // navigation finishes, we unelide the URL if it was a full cross-document
  // navigation. Once the user interacts with the page, we create and run
  // |elide_after_interaction_animation_| to elide the URL. If
  // ShouldRevealPathQueryRefOnHover() is also enabled, we defer the creation of
  // |hover_elide_or_unelide_animation_| until the user interacts with the page
  // as well, since we don't want to do any hover animations until the URL has
  // been elided after user interaction. After the first user interaction,
  // |elide_after_interaction_animation_| doesn't run again until it's
  // re-created after the next navigation, and
  // |hover_elide_or_unelide_animation_| behaves as described above for the rest
  // of the navigation. There are 2 separate animations (one for
  // after-interaction and one hovering) so that the state of the
  // after-interaction animation can be queried to know when the user has or has
  // not already interacted with the page.
  std::unique_ptr<ElideAnimation> elide_after_interaction_animation_;

  // Selection persisted across temporary text changes, like popup suggestions.
  std::vector<gfx::Range> saved_temporary_selection_;

  // Holds the user's selection across focus changes.  There is only a saved
  // selection if this range IsValid().
  std::vector<gfx::Range> saved_selection_for_focus_change_;

  // Tracking state before and after a possible change.
  State state_before_change_;
  bool ime_composing_before_change_ = false;

  // |location_bar_view_| can be NULL in tests.
  LocationBarView* location_bar_view_;

#if defined(OS_CHROMEOS)
  // True if the IME candidate window is open. When this is true, we want to
  // avoid showing the popup. So far, the candidate window is detected only
  // on Chrome OS.
  bool ime_candidate_window_open_ = false;
#endif

  // True if any mouse button is currently depressed.
  bool is_mouse_pressed_ = false;

  // Applies a minimum threshold to drag events after unelision. Because the
  // text shifts after unelision, we don't want unintentional mouse drags to
  // change the selection.
  bool filter_drag_events_for_unelision_ = false;

  // Should we select all the text when we see the mouse button get released?
  // We select in response to a click that focuses the omnibox, but we defer
  // until release, setting this variable back to false if we saw a drag, to
  // allow the user to select just a portion of the text.
  bool select_all_on_mouse_release_ = false;

  // Indicates if we want to select all text in the omnibox when we get a
  // GESTURE_TAP. We want to select all only when the textfield is not in focus
  // and gets a tap. So we use this variable to remember focus state before tap.
  bool select_all_on_gesture_tap_ = false;

  // Keep track of the word that would be selected if URL is unelided between
  // a single and double click. This is an edge case where the elided URL is
  // selected. On the double click, unelision is performed in between the first
  // and second clicks. This results in both the wrong word to be selected and
  // the wrong selection length. For example, if example.com is shown and you
  // try to double click on the "x", it unelides to https://example.com after
  // the first click, resulting in "https" being selected.
  size_t next_double_click_selection_len_ = 0;
  size_t next_double_click_selection_offset_ = 0;

  // The time of the first character insert operation that has not yet been
  // painted. Used to measure omnibox responsiveness with a histogram.
  base::TimeTicks insert_char_time_;

  // The state machine for logging the Omnibox.CharTypedToRepaintLatency
  // histogram.
  enum {
    NOT_ACTIVE,           // Not currently tracking a char typed event.
    CHAR_TYPED,           // Character was typed.
    ON_PAINT_CALLED,      // Character was typed and OnPaint() called.
    COMPOSITING_COMMIT,   // Compositing was committed after OnPaint().
    COMPOSITING_STARTED,  // Compositing was started.
  } latency_histogram_state_;

  // The currently selected match, if any, with additional labelling text
  // such as the document title and the type of search, for example:
  // "Google https://google.com location from bookmark", or
  // "cats are liquid search suggestion".
  base::string16 friendly_suggestion_text_;

  // The number of added labelling characters before editable text begins.
  // For example,  "Google https://google.com location from history",
  // this is set to 7 (the length of "Google ").
  int friendly_suggestion_text_prefix_length_;

  ScopedObserver<ui::Compositor, ui::CompositorObserver>
      scoped_compositor_observer_{this};
  ScopedObserver<TemplateURLService, TemplateURLServiceObserver>
      scoped_template_url_service_observer_{this};

  // Send tab to self submenu.
  std::unique_ptr<send_tab_to_self::SendTabToSelfSubMenuModel>
      send_tab_to_self_sub_menu_model_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<OmniboxViewViews> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_
