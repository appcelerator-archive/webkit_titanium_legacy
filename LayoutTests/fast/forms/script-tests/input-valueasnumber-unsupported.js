description('Tests for .valueAsNumber with non-supported input types.');

var input = document.createElement('input');
input.type = 'range';
document.body.appendChild(input);

function checkValueAsNumber(type) {
    debug('Check for type=' + type);
    input.value = '0';
    input.type = type;
    shouldBe('input.valueAsNumber', 'Number.NaN');
    shouldThrow('input.valueAsNumber = 0', '"Error: INVALID_STATE_ERR: DOM Exception 11"');
}

checkValueAsNumber('button');
checkValueAsNumber('checkbox');
checkValueAsNumber('color');
checkValueAsNumber('email');
checkValueAsNumber('file');
checkValueAsNumber('hidden');
checkValueAsNumber('image');
checkValueAsNumber('khtml_isindex');
checkValueAsNumber('password');
checkValueAsNumber('radio');
checkValueAsNumber('reset');
checkValueAsNumber('search');
checkValueAsNumber('submit');
checkValueAsNumber('tel');
checkValueAsNumber('text');
checkValueAsNumber('url');

var successfullyParsed = true;
