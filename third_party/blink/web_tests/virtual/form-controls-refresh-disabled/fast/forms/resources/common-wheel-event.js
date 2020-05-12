async function dispatchWheelEvent(element, deltaX, deltaY)
{
    var rect = element.getClientRects()[0]

    const x = rect.left;
    const y = rect.top;
    const source = GestureSourceType.MOUSE_INPUT;
    const speed = SPEED_INSTANT;
    const precise_scrolling_deltas = false;

    await mouseMoveTo(x, y);
    await smoothScrollWithXY(deltaX, deltaY, x, y, source, speed,
        precise_scrolling_deltas);
}

var input;
async function testWheelEvent(parameters)
{
    window.jsTestIsAsync = true;
    var inputType = parameters['inputType'];
    var initialValue = parameters['initialValue'];
    var stepUpValue1 = parameters['stepUpValue1'];
    var stepUpValue2 = parameters['stepUpValue2'];
    description('Test for wheel operations for &lt;input type=' + inputType + '>');
    var parent = document.createElement('div');
    document.body.appendChild(parent);
    parent.innerHTML = '<input type=' + inputType + ' id=test value="' + initialValue + '"> <input id=another>';
    input = document.getElementById('test');
    input.focus();

    debug('Initial value is ' + initialValue + '. We\'ll wheel up by 1:');
    await dispatchWheelEvent(input, 0, -1);
    shouldBeEqualToString('input.value', stepUpValue1);

    debug('Wheel up by 100:');
    await dispatchWheelEvent(input, 0, -100);
    shouldBeEqualToString('input.value', stepUpValue2);

    debug('Wheel down by 1:');
    await dispatchWheelEvent(input, 0, 1);
    shouldBeEqualToString('input.value', stepUpValue1);

    debug('Wheel down by 256:');
    await dispatchWheelEvent(input, 0, 256);
    shouldBeEqualToString('input.value', initialValue);

    debug('Disabled input element:');
    input.disabled = true;
    await dispatchWheelEvent(input, 0, -1);
    shouldBeEqualToString('input.value', initialValue);
    input.removeAttribute('disabled');


    debug('Read-only input element:');
    input.readOnly = true;
    await dispatchWheelEvent(input, 0, -1);
    shouldBeEqualToString('input.value', initialValue);
    input.readOnly = false;

    debug('No focus:');
    document.getElementById('another').focus();
    await dispatchWheelEvent(input, 0, -1);
    shouldBeEqualToString('input.value', initialValue);

    parent.parentNode.removeChild(parent);
    finishJSTest();
}
