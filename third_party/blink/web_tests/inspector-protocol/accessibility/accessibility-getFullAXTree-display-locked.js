(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='activatable' style='subtree-visibility: hidden-matchable'>
      locked
      <div id='child'>
        child
        <div id='grandChild'>grandChild</div>
      </div>
      <div id='invisible' style='display:none'>invisible</div>
      <div id='nested' style='subtree-visibility: hidden-matchable'>nested</div>
      text
    </div>
    <div id='nonActivatable' style='subtree-visibility: hidden'>nonActivatable text</div>
    <div id='normal'>normal text</div>
  `, 'Tests accessibility values of display locked nodes');
  const dumpAccessibilityNodesFromList =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodesFromList.js'))(testRunner, session);

  const {result} = await dp.Accessibility.getFullAXTree();
  dumpAccessibilityNodesFromList(result.nodes);
});
