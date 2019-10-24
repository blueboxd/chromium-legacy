load('//versioned/vars/ci.star', 'vars')
vars.bucket.set('ci-stable')

load('//versioned/milestones.star', milestone='stable')
exec('//versioned/milestones/%s/buckets/ci.star' % milestone)
