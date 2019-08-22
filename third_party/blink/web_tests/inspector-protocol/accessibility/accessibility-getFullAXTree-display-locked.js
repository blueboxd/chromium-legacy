(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='activatable' rendersubtree='invisible-activatable'>
      locked
      <div id='child'>
        child
        <div id='grandChild'>grandChild</div>
      </div>
      <div id='invisible' style='display:none'>invisible</div>
      <div id='nested' rendersubtree='invisible-activatable'>nested</div>
      text
    </div>
    <div id='nonActivatable' rendersubtree='invisible'>nonActivatable text</div>
    <div id='normal'>normal text</div>
    <script>
      activatable.getBoundingClientRect(); // Force layout
    </script>
  `, 'Tests accessibility values of display locked nodes');
  const dumpAccessibilityNodesFromList =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodesFromList.js'))(testRunner, session);

  const {result} = await dp.Accessibility.getFullAXTree();
  dumpAccessibilityNodesFromList(result.nodes);
});
