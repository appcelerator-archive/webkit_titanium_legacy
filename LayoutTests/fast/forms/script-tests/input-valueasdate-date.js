description('Tests for .valueAsDate with &lt;input type=date>.');

var input = document.createElement('input');
input.type = 'date';

function valueAsDateFor(stringValue) {
    input.value = stringValue;
    return input.valueAsDate;
}

function setValueAsDateAndGetValue(year, month, day) {
    var date = new Date();
    date.setTime(Date.UTC(year, month, day));
    input.valueAsDate = date;
    return input.value;
}

shouldBe('valueAsDateFor("")', 'null');
shouldBe('valueAsDateFor("1969-12-31").getTime()', 'Date.UTC(1969, 11, 31)');
shouldBe('valueAsDateFor("1970-01-01").getTime()', 'Date.UTC(1970, 0, 1)');
shouldBe('valueAsDateFor("2009-12-22").getTime()', 'Date.UTC(2009, 11, 22)');

shouldBe('setValueAsDateAndGetValue(1969, 11, 1)', '"1969-12-01"');
shouldBe('setValueAsDateAndGetValue(1970, 0, 1)', '"1970-01-01"');
shouldBe('setValueAsDateAndGetValue(2009, 11, 31)', '"2009-12-31"');
shouldBe('setValueAsDateAndGetValue(10000, 0, 1)', '"10000-01-01"');

shouldBe('setValueAsDateAndGetValue(794, 9, 22)', '""');
shouldBe('setValueAsDateAndGetValue(1582, 8, 30)', '""');
shouldBe('setValueAsDateAndGetValue(1582, 9, 15)', '"1582-10-15"');
shouldBe('setValueAsDateAndGetValue(1582, 9, 31)', '"1582-10-31"');
shouldBe('setValueAsDateAndGetValue(275760, 8, 13)', '"275760-09-13"');
shouldBe('setValueAsDateAndGetValue(275760, 8, 14)', '""'); // Date of JavaScript can't represent this.

debug('Invalid objects:');
shouldBe('input.value = "2010-01-01"; input.valueAsDate = document; input.value', '""');
shouldBe('input.value = "2010-01-01"; input.valueAsDate = null; input.value', '""');

var successfullyParsed = true;
