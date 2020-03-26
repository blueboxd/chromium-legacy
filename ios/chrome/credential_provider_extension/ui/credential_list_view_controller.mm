// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialListViewController () <UISearchResultsUpdating>

// Search controller that contains search bar.
@property(nonatomic, strong) UISearchController* searchController;

@end

// TODO(crbug.com/1045454): Implement this view controller.
@implementation CredentialListViewController

@synthesize delegate;

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.navigationItem.rightBarButtonItem = [self navigationCancelButton];

  self.searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController.searchResultsUpdater = self;
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.tableView.tableHeaderView = self.searchController.searchBar;
  self.navigationController.navigationBar.translucent = NO;

  // Presentation of searchController will walk up the view controller hierarchy
  // until it finds the root view controller or one that defines a presentation
  // context. Make this class the presentation context so that the search
  // controller does not present on top of the navigation controller.
  self.definesPresentationContext = YES;
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  [self.delegate updateResultsWithFilter:searchController.searchBar.text];
}

#pragma mark - Private

// Creates a cancel button for the navigation item.
- (UIBarButtonItem*)navigationCancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self.delegate
                           action:@selector(navigationCancelButtonWasPressed:)];
  return cancelButton;
}

@end
