vars = struct(
    ref = 'refs/heads/master',
    ci_bucket = 'ci',
    ci_poller = 'master-gitiles-trigger',
    try_bucket = 'try',
    cq_group = 'cq',
    cq_ref_regexp = 'refs/heads/.+',
    main_console_name = 'main',
    main_console_title = 'Chromium Main Console',
    # Delete this line for branches
    tree_status_host = 'chromium-status.appspot.com/',
)
