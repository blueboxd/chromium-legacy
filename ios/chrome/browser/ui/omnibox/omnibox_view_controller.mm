// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kClearButtonSize = 28.0f;

}  // namespace

@interface OmniboxViewController ()

// Override of UIViewController's view with a different type.
@property(nonatomic, strong) OmniboxContainerView* view;

// Whether the default search engine supports search-by-image. This controls the
// edit menu option to do an image search.
@property(nonatomic, assign) BOOL searchByImageEnabled;

@property(nonatomic, assign) BOOL incognito;

@end

@implementation OmniboxViewController
@dynamic view;

- (instancetype)initWithIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _incognito = isIncognito;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  UIColor* textColor = self.incognito ? [UIColor whiteColor]
                                      : [UIColor colorWithWhite:0 alpha:0.7];
  UIColor* textFieldTintColor = self.incognito
                                    ? [UIColor whiteColor]
                                    : UIColorFromRGB(kLocationBarTintBlue);
  UIColor* iconTintColor;
  if (base::FeatureList::IsEnabled(kNewOmniboxPopupLayout)) {
    iconTintColor = self.incognito
                        ? [UIColor.whiteColor colorWithAlphaComponent:0.7]
                        : [UIColor.blackColor colorWithAlphaComponent:0.5];
  } else {
    iconTintColor = self.incognito ? [UIColor whiteColor]
                                   : [UIColor colorWithWhite:0 alpha:0.7];
  }

  self.view = [[OmniboxContainerView alloc]
      initWithFrame:CGRectZero
          textColor:textColor
      textFieldTint:textFieldTintColor
           iconTint:iconTintColor];
  self.view.incognito = self.incognito;

  SetA11yLabelAndUiAutomationName(self.textField, IDS_ACCNAME_LOCATION,
                                  @"Address");
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Add Paste and Go option to the editing menu
  UIMenuController* menu = [UIMenuController sharedMenuController];
  if (base::FeatureList::IsEnabled(kCopiedContentBehavior)) {
    UIMenuItem* searchCopiedImage = [[UIMenuItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_IMAGE)
               action:@selector(searchCopiedImage:)];
    UIMenuItem* visitCopiedLink = [[UIMenuItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_VISIT_COPIED_LINK)
               action:@selector(visitCopiedLink:)];
    UIMenuItem* searchCopiedText = [[UIMenuItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_TEXT)
               action:@selector(searchCopiedText:)];
    [menu
        setMenuItems:@[ searchCopiedImage, visitCopiedLink, searchCopiedText ]];
  } else {
    UIMenuItem* pasteAndGo = [[UIMenuItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_PASTE_AND_GO)
               action:NSSelectorFromString(@"pasteAndGo:")];
    [menu setMenuItems:@[ pasteAndGo ]];
  }

  self.textField.placeholderTextColor = [self placeholderAndClearButtonColor];
  self.textField.placeholder = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  [self setupClearButton];

  [NSNotificationCenter.defaultCenter
      addObserver:self
         selector:@selector(textInputModeDidChange)
             name:UITextInputCurrentInputModeDidChangeNotification
           object:nil];

  // TODO(crbug.com/866446): Use UITextFieldDelegate instead.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(textFieldDidBeginEditing)
             name:UITextFieldTextDidBeginEditingNotification
           object:self.textField];

  [self updateLeadingImageVisibility];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  [self.view attachLayoutGuides];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  self.textField.selectedTextRange =
      [self.textField textRangeFromPosition:self.textField.beginningOfDocument
                                 toPosition:self.textField.beginningOfDocument];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateLeadingImageVisibility];
}

#pragma mark - public methods

- (OmniboxTextFieldIOS*)textField {
  return self.view.textField;
}

#pragma mark - OmniboxConsumer

- (void)updateAutocompleteIcon:(UIImage*)icon {
  [self.view setLeadingImage:icon];
}

- (void)updateSearchByImageSupported:(BOOL)searchByImageSupported {
  self.searchByImageEnabled = searchByImageSupported;
}

#pragma mark - EditViewAnimatee

- (void)setLeadingIconFaded:(BOOL)faded {
  [self.view setLeadingImageAlpha:faded ? 0 : 1];
}

- (void)setClearButtonFaded:(BOOL)faded {
  self.textField.rightView.alpha = faded ? 0 : 1;
}

#pragma mark - LocationBarOffsetProvider

- (CGFloat)xOffsetForString:(NSString*)string {
  return [self.textField offsetForString:string];
}

#pragma mark - private

- (void)updateLeadingImageVisibility {
  BOOL newOmniboxPopupLayout =
      base::FeatureList::IsEnabled(kNewOmniboxPopupLayout);
  [self.view setLeadingImageHidden:!newOmniboxPopupLayout &&
                                   !IsRegularXRegularSizeClass(self)];
}

// Tint color for the textfield placeholder and the clear button.
- (UIColor*)placeholderAndClearButtonColor {
  return self.incognito
             ? [UIColor colorWithWhite:1 alpha:0.5]
             : [UIColor colorWithWhite:0 alpha:kOmniboxPlaceholderAlpha];
}

#pragma mark notification callbacks

// Called on UITextFieldTextDidBeginEditingNotification for self.textField.
- (void)textFieldDidBeginEditing {
  // Update the clear button state.
  [self updateClearButtonVisibility];
  [self.view setLeadingImage:self.textField.text.length
                                 ? self.defaultLeadingImage
                                 : self.emptyTextLeadingImage];

  self.semanticContentAttribute = [self.textField bestSemanticContentAttribute];
}

// Called on UITextInputCurrentInputModeDidChangeNotification for self.textField
- (void)textInputModeDidChange {
  // Only respond to language changes when the omnibox is first responder.
  if (![self.textField isFirstResponder]) {
    return;
  }

  [self.textField updateTextDirection];
  self.semanticContentAttribute = [self.textField bestSemanticContentAttribute];

  [self.delegate omniboxViewControllerTextInputModeDidChange:self];
}

#pragma mark clear button

// Omnibox uses a custom clear button. It has a custom tint and image, but
// otherwise it should act exactly like a system button. To achieve this, a
// custom button is used as the |rightView|. Textfield's setRightViewMode: is
// used to make the button invisible when the textfield is empty; the visibility
// is updated on textfield text changes and clear button presses.
- (void)setupClearButton {
  // Do not use the system clear button. Use a custom "right view" instead.
  // Note that |rightView| is an incorrect name, it's really a trailing view.
  [self.textField setClearButtonMode:UITextFieldViewModeNever];
  [self.textField setRightViewMode:UITextFieldViewModeAlways];

  UIButton* clearButton = [UIButton buttonWithType:UIButtonTypeSystem];
  clearButton.frame = CGRectMake(0, 0, kClearButtonSize, kClearButtonSize);
  [clearButton setImage:[self clearButtonIcon] forState:UIControlStateNormal];
  [clearButton addTarget:self
                  action:@selector(clearButtonPressed)
        forControlEvents:UIControlEventTouchUpInside];
  self.textField.rightView = clearButton;

  clearButton.tintColor = [self placeholderAndClearButtonColor];
  SetA11yLabelAndUiAutomationName(clearButton, IDS_IOS_ACCNAME_CLEAR_TEXT,
                                  @"Clear Text");

  // Observe text changes to show the clear button when there is text and hide
  // it when the textfield is empty.
  [self.textField addTarget:self
                     action:@selector(textFieldDidChange:)
           forControlEvents:UIControlEventEditingChanged];
}

- (UIImage*)clearButtonIcon {
  UIImage* image = [[UIImage imageNamed:@"omnibox_clear_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  return image;
}

- (void)clearButtonPressed {
  // Emulate a system button clear callback.
  BOOL shouldClear =
      [self.textField.delegate textFieldShouldClear:self.textField];
  if (shouldClear) {
    [self.textField setText:@""];
    // Calling setText: does not trigger UIControlEventEditingChanged, so update
    // the clear button visibility manually.
    [self.textField sendActionsForControlEvents:UIControlEventEditingChanged];
  }
}

// Called on textField's UIControlEventEditingChanged.
- (void)textFieldDidChange:(UITextField*)textField {
  // If the text is empty, update the leading image.
  if (self.textField.text.length == 0) {
    [self.view setLeadingImage:self.emptyTextLeadingImage];
  }

  [self updateClearButtonVisibility];
  self.semanticContentAttribute = [self.textField bestSemanticContentAttribute];
}

// Hides the clear button if the textfield is empty; shows it otherwise.
- (void)updateClearButtonVisibility {
  BOOL hasText = self.textField.text.length > 0;
  [self.textField setRightViewMode:hasText ? UITextFieldViewModeAlways
                                           : UITextFieldViewModeNever];
}

// Handle the updates to semanticContentAttribute by passing the changes along
// to the necessary views.
- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  _semanticContentAttribute = semanticContentAttribute;

  if (!base::FeatureList::IsEnabled(kNewOmniboxPopupLayout)) {
    return;
  }

  self.view.semanticContentAttribute = self.semanticContentAttribute;
  self.textField.semanticContentAttribute = self.semanticContentAttribute;
}

#pragma mark - UIMenuItem

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  // Remove with flag kCopiedContentBehavior
  if (action == @selector(pasteAndGo:)) {
    DCHECK(!base::FeatureList::IsEnabled(kCopiedContentBehavior));
    return UIPasteboard.generalPasteboard.string.length > 0;
  }

  if (action == @selector(searchCopiedImage:) ||
      action == @selector(visitCopiedLink:) ||
      action == @selector(searchCopiedText:)) {
    ClipboardRecentContent* clipboardRecentContent =
        ClipboardRecentContent::GetInstance();
    if (self.searchByImageEnabled &&
        clipboardRecentContent->GetRecentImageFromClipboard().has_value()) {
      return action == @selector(searchCopiedImage:);
    }
    if (clipboardRecentContent->GetRecentURLFromClipboard().has_value()) {
      return action == @selector(visitCopiedLink:);
    }
    if (clipboardRecentContent->GetRecentTextFromClipboard().has_value()) {
      return action == @selector(searchCopiedText:);
    }
    return NO;
  }
  return NO;
}

- (void)searchCopiedImage:(id)sender {
  DCHECK(base::FeatureList::IsEnabled(kCopiedContentBehavior));
  if (base::Optional<gfx::Image> optionalImage =
          ClipboardRecentContent::GetInstance()
              ->GetRecentImageFromClipboard()) {
    UIImage* image = optionalImage.value().ToUIImage();
    [self.dispatcher searchByImage:image];
    [self.dispatcher cancelOmniboxEdit];
  }
}

- (void)visitCopiedLink:(id)sender {
  [self pasteAndGo:sender];
}

- (void)searchCopiedText:(id)sender {
  [self pasteAndGo:sender];
}

// Both actions are performed the same, but need to be enabled differently,
// so we need two different selectors.
- (void)pasteAndGo:(id)sender {
  NSString* query;
  if (base::FeatureList::IsEnabled(kCopiedContentBehavior)) {
    ClipboardRecentContent* clipboardRecentContent =
        ClipboardRecentContent::GetInstance();
    if (base::Optional<GURL> optionalUrl =
            clipboardRecentContent->GetRecentURLFromClipboard()) {
      query = base::SysUTF8ToNSString(optionalUrl.value().spec());
    } else if (base::Optional<base::string16> optionalText =
                   clipboardRecentContent->GetRecentTextFromClipboard()) {
      query = base::SysUTF16ToNSString(optionalText.value());
    }
  } else {
    query = UIPasteboard.generalPasteboard.string;
  }
  [self.dispatcher loadQuery:query immediately:YES];
  [self.dispatcher cancelOmniboxEdit];
}

@end
