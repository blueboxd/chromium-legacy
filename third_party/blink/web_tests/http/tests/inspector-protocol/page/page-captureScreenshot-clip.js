(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <style>div.above {
      border: 2px solid blue;
      background: red;
      height: 15000px;
    }
    div.to-screenshot {
      border: 2px solid blue;
      background: green;
      width: 50px;
      height: 50px;
    }
    body {
      margin: 0;
    }
    </style>
    <div style="height: 14px">oooo</div>
    <div class="above"></div>
    <div class="to-screenshot"></div>`, 'Tests that screenshot works with clip');

  const json = await session.evaluate(`
    (() => {
      const div = document.querySelector('div.to-screenshot');
      const box = div.getBoundingClientRect();
      window.scrollTo(0, 15000);
      return JSON.stringify({x: box.left, y: box.top, width: box.width, height: box.height});
    })()
  `);
  testRunner.log(json);

  const clip = JSON.parse(json);
  testRunner.log(await dp.Page.captureScreenshot({format: 'png', clip: {...clip, scale: 1}}));

  testRunner.completeTest();
})