description('Tests for .valueAsNumber with &lt;input type=datetime>.');

var input = document.createElement('input');
input.type = 'datetime';

function valueAsNumberFor(stringValue) {
    input.value = stringValue;
    return input.valueAsNumber;
}

function setValueAsNumberAndGetValue(year, month, day, hour, minute, second, msec) {
    input.valueAsNumber = Date.UTC(year, month, day, hour, minute, second, msec);
    return input.value;
}

shouldBe('valueAsNumberFor("")', 'Number.NaN');
shouldBe('valueAsNumberFor("1969-12-31T12:34:56.789Z")', 'Date.UTC(1969, 11, 31, 12, 34, 56, 789)');
shouldBe('valueAsNumberFor("1970-01-01T00:00:00.000Z")', 'Date.UTC(1970, 0, 1, 0, 0, 0)');
shouldBe('valueAsNumberFor("2009-12-22T11:32:11Z")', 'Date.UTC(2009, 11, 22, 11, 32, 11)');

shouldBe('setValueAsNumberAndGetValue(1969, 11, 1, 0, 0, 0, 0)', '"1969-12-01T00:00Z"');
shouldBe('setValueAsNumberAndGetValue(1970, 0, 1, 10, 1, 0, 100)', '"1970-01-01T10:01:00.100Z"');
shouldBe('setValueAsNumberAndGetValue(2009, 11, 31, 23, 59, 59, 999)', '"2009-12-31T23:59:59.999Z"');
shouldBe('setValueAsNumberAndGetValue(10000, 0, 1, 12, 0, 1, 0)', '"10000-01-01T12:00:01Z"');

shouldBe('setValueAsNumberAndGetValue(794, 9, 22, 0, 0, 0, 0)', '""');
shouldBe('setValueAsNumberAndGetValue(1582, 9, 14, 23, 59, 59, 999)', '""');
shouldBe('setValueAsNumberAndGetValue(1582, 9, 15, 0, 0, 0, 0)', '"1582-10-15T00:00Z"');
shouldBe('setValueAsNumberAndGetValue(275760, 8, 12, 0, 0, 0, 1)', '"275760-09-12T00:00:00.001Z"');
shouldBe('setValueAsNumberAndGetValue(275760, 8, 13, 0, 0, 0, 0)', '"275760-09-13T00:00Z"');
// Date.UTC() of V8 throws an exception for the following value though JavaScriptCore doesn't.
// shouldBe('setValueAsNumberAndGetValue(275760, 8, 13, 0, 0, 0, 1)', '"275760-09-13T00:00:00.001Z"');

debug('Tests to set invalid values to valueAsNumber:');
shouldBe('input.value = ""; input.valueAsNumber = null; input.value', '"1970-01-01T00:00Z"');
shouldThrow('input.valueAsNumber = "foo"', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = NaN', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Number.NaN', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Infinity', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Number.POSITIVE_INFINITY', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow('input.valueAsNumber = Number.NEGATIVE_INFINITY', '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');

debug('Step attribute value and string representation:');
// If the step attribute value is 1 second and the second part is 0, we should show the second part.
shouldBe('input.step = "1"; setValueAsNumberAndGetValue(2010, 0, 21, 0, 0, 0, 0)', '"2010-01-21T00:00:00Z"');
// If the step attribute value is 0.001 second and the millisecond part is 0, we should show the millisecond part.
shouldBe('input.step = "0.001"; setValueAsNumberAndGetValue(2010, 0, 21, 0, 0, 0, 0)', '"2010-01-21T00:00:00.000Z"');

var successfullyParsed = true;
