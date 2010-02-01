description('Tests for .valueAsNumber with &lt;input type=month>.');

var input = document.createElement('input');
input.type = 'month';

function valueAsNumberFor(stringValue) {
    input.value = stringValue;
    return input.valueAsNumber;
}

function setValueAsNumberAndGetValue(y, m) {
    input.valueAsNumber = (y - 1970) * 12 + m - 1;
    return input.value;
}

shouldBe('valueAsNumberFor("")', 'Number.NaN');
shouldBe('valueAsNumberFor("1969-01")', '-12');
shouldBe('valueAsNumberFor("1969-12")', '-1');
shouldBe('valueAsNumberFor("1970-01")', '0');
shouldBe('valueAsNumberFor("1970-12")', '11');
shouldBe('valueAsNumberFor("1971-01")', '12');
shouldBe('valueAsNumberFor("2009-12")', '(2009-1970)*12+11');

shouldBe('input.valueAsNumber = -1; input.value', '"1969-12"');
shouldBe('input.valueAsNumber = 0; input.value', '"1970-01"');
shouldBe('setValueAsNumberAndGetValue(2009, 12)', '"2009-12"');
shouldBe('setValueAsNumberAndGetValue(10000, 1)', '"10000-01"');

shouldBe('setValueAsNumberAndGetValue(794, 9)', '""');
shouldBe('setValueAsNumberAndGetValue(1582, 9)', '""');
shouldBe('setValueAsNumberAndGetValue(1582, 10)', '"1582-10"');
shouldBe('setValueAsNumberAndGetValue(1582, 11)', '"1582-11"');
shouldBe('setValueAsNumberAndGetValue(275760, 9)', '"275760-09"');
shouldBe('setValueAsNumberAndGetValue(2147483647, 12)', '"2147483647-12"');

// Date.UTC() of V8 throws an exception for the following value though JavaScriptCore doesn't.
// shouldBe('setValueAsNumberAndGetValue(275760, 8, 14)', '"275760-09"');

debug('Tests to set invalid values to valueAsNumber:');
shouldBe('input.value = ""; input.valueAsNumber = null; input.value', '"1970-01"');
shouldThrow('input.valueAsNumber = "foo"', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = NaN', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Number.NaN', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Infinity', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Number.POSITIVE_INFINITY', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Number.NEGATIVE_INFINITY', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');

var successfullyParsed = true;
