load('//versioned/vars/ci.star', 'vars')
vars.bucket.set('ci-stable')
vars.poller.set('stable-gitiles-trigger')

load('//lib/builders.star', 'defaults')
defaults.pool.set('luci.chromium.ci')

load('//versioned/milestones.star', milestone='stable')
exec('//versioned/milestones/%s/buckets/ci.star' % milestone)
