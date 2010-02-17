description('Check stepMismatch results for type=date, datetime, datetime-local, month, time, week.');
// FIXME: Merge other ValidityState-stepMismatch-* into this.

var input = document.createElement('input');

function stepMismatchFor(value, step, min) {
    input.value = value;
    input.step = step;
    input.min = min;
    return input.validity.stepMismatch;
}

debug('Date type');
input.type = 'date';
debug('Empty values');
shouldBeFalse('stepMismatchFor("", null, null)');
shouldBeFalse('stepMismatchFor("", "2", "1969-12-31")');
debug('Normal step values');
shouldBeTrue('stepMismatchFor("2010-02-10", "2", "2010-02-09")');
shouldBeFalse('stepMismatchFor("2010-02-09", "2", "2010-02-09")');
shouldBeFalse('stepMismatchFor("2010-02-11", "2", "2010-02-09")');
shouldBeTrue('stepMismatchFor("1800-11-11", "3", "1800-11-09")');
shouldBeFalse('stepMismatchFor("1800-11-09", "3", "1800-11-09")');
shouldBeFalse('stepMismatchFor("1800-11-12", "3", "1800-11-09")');
shouldBeTrue('stepMismatchFor("275760-09-13", "3", "275760-09-11")');
shouldBeFalse('stepMismatchFor("275760-09-13", "2", "275760-09-11")');
debug('Implicit step base');
shouldBeTrue('stepMismatchFor("1970-01-02", "2", null)');
shouldBeFalse('stepMismatchFor("1970-01-03", "2", null)');
debug('Fractional step values');
shouldBeFalse('stepMismatchFor("2010-02-10", "0.1", "2010-02-09")');
shouldBeFalse('stepMismatchFor("2010-02-10", "1.1", "2010-02-09")');
shouldBeTrue('stepMismatchFor("2010-02-10", "1.9", "2010-02-09")');
debug('Invalid or no step values');
shouldBeFalse('stepMismatchFor("2010-02-10", null, "2010-02-09")');
shouldBeFalse('stepMismatchFor("2010-02-10", "-1", "2010-02-09")');
shouldBeFalse('stepMismatchFor("2010-02-10", "foo", "2010-02-09")');
debug('Special step value');
shouldBeFalse('stepMismatchFor("2010-02-10", "any", "2010-02-09")');

debug('');
debug('Datetime type');
input.type = 'datetime';
debug('Empty values');
shouldBeFalse('stepMismatchFor("", null, null)');
shouldBeFalse('stepMismatchFor("", "2", "1969-12-31T12:34:56Z")');
debug('Normal step values');
shouldBeFalse('stepMismatchFor("2010-02-09T12:34:55Z", "1", "2010-02-09T12:34:56Z")');
shouldBeTrue('stepMismatchFor("2010-02-09T12:34:55.001Z", "1", "2010-02-09T12:34:56Z")');
shouldBeFalse('stepMismatchFor("2010-02-09T12:34:56.001Z", "0.001", "2010-02-09T12:34:56Z")');
shouldBeTrue('stepMismatchFor("2010-02-09T12:34:55Z", "0.333", "2010-02-09T12:34:56Z")');
shouldBeFalse('stepMismatchFor("2010-02-09T12:34:55.001Z", "0.333", "2010-02-09T12:34:56Z")');
shouldBeFalse('stepMismatchFor("2010-02-09T12:34Z", "86400", "2010-02-08T12:34Z")');
shouldBeTrue('stepMismatchFor("2010-02-09T12:34:56Z", "86400", "2010-02-08T12:34Z")');
shouldBeTrue('stepMismatchFor("275760-09-13T00:00Z", "3", "275760-09-13T23:50:50Z")');
shouldBeFalse('stepMismatchFor("275760-09-13T00:00Z", "2", "275760-09-13T23:50:50Z")');
debug('Implicit step base');
shouldBeFalse('stepMismatchFor("1970-01-01T12:34Z", "120", null)');
shouldBeTrue('stepMismatchFor("1970-01-01T12:35Z", "120", null)');
debug('Small step values');
shouldBeFalse('stepMismatchFor("2010-02-10T12:34:56.000Z", "0.0003", "2010-02-10T12:34.55.000Z")');
shouldBeTrue('stepMismatchFor("2010-02-10T12:34:55.001Z", "0.0019", "2010-02-10T12:34.55.000Z")');
debug('Invalid or no step values');
shouldBeFalse('stepMismatchFor("2010-02-10T12:34Z", null, "2010-02-09T12:34Z")');
shouldBeTrue('stepMismatchFor("2010-02-10T12:34:56Z", null, "2010-02-09T12:34Z")');
shouldBeFalse('stepMismatchFor("2010-02-10T12:34Z", "-1", "2010-02-09T12:34Z")');
shouldBeFalse('stepMismatchFor("2010-02-10T12:34Z", "foo", "2010-02-09T12:34Z")');
debug('Special step value');
shouldBeFalse('stepMismatchFor("2010-02-09T12:34Z", "any", "2010-02-09T12:34Z")');

debug('');
debug('Datetime-local type');
input.type = 'datetime-local';
debug('Empty values');
shouldBeFalse('stepMismatchFor("", null, null)');
shouldBeFalse('stepMismatchFor("", "2", "1969-12-31T12:34:56")');
debug('Normal step values');
shouldBeFalse('stepMismatchFor("2010-02-09T12:34:55", "1", "2010-02-09T12:34:56")');
shouldBeTrue('stepMismatchFor("2010-02-09T12:34:55.001", "1", "2010-02-09T12:34:56")');
shouldBeFalse('stepMismatchFor("2010-02-09T12:34:56.001", "0.001", "2010-02-09T12:34:56")');
shouldBeTrue('stepMismatchFor("2010-02-09T12:34:55", "0.333", "2010-02-09T12:34:56")');
shouldBeFalse('stepMismatchFor("2010-02-09T12:34:55.001", "0.333", "2010-02-09T12:34:56")');
shouldBeFalse('stepMismatchFor("2010-02-09T12:34", "86400", "2010-02-08T12:34")');
shouldBeTrue('stepMismatchFor("2010-02-09T12:34:56", "86400", "2010-02-08T12:34")');
shouldBeTrue('stepMismatchFor("275760-09-13T00:00", "3", "275760-09-13T23:50:50")');
shouldBeFalse('stepMismatchFor("275760-09-13T00:00", "2", "275760-09-13T23:50:50")');
debug('Implicit step base');
shouldBeFalse('stepMismatchFor("1970-01-01T12:34", "120", null)');
shouldBeTrue('stepMismatchFor("1970-01-01T12:35", "120", null)');
debug('Small step values');
shouldBeFalse('stepMismatchFor("2010-02-10T12:34:56.000", "0.0003", "2010-02-10T12:34.55.000")');
shouldBeTrue('stepMismatchFor("2010-02-10T12:34:55.001", "0.0019", "2010-02-10T12:34.55.000")');
debug('Invalid or no step values');
shouldBeFalse('stepMismatchFor("2010-02-10T12:34", null, "2010-02-09T12:34")');
shouldBeTrue('stepMismatchFor("2010-02-10T12:34:56", null, "2010-02-09T12:34")');
shouldBeFalse('stepMismatchFor("2010-02-10T12:34", "-1", "2010-02-09T12:34")');
shouldBeFalse('stepMismatchFor("2010-02-10T12:34", "foo", "2010-02-09T12:34")');
debug('Special step value');
shouldBeFalse('stepMismatchFor("2010-02-09T12:34", "any", "2010-02-09T12:34")');

debug('');
debug('Month type');
input.type = 'month';
debug('Empty values');
shouldBeFalse('stepMismatchFor("", null, null)');
shouldBeFalse('stepMismatchFor("", "2", "1969-12")');
debug('Normal step values');
shouldBeTrue('stepMismatchFor("2010-03", "2", "2010-02")');
shouldBeFalse('stepMismatchFor("2010-02", "2", "2010-02")');
shouldBeFalse('stepMismatchFor("2010-04", "2", "2010-02")');
shouldBeTrue('stepMismatchFor("1800-11", "3", "1800-09")');
shouldBeFalse('stepMismatchFor("1800-09", "3", "1800-09")');
shouldBeFalse('stepMismatchFor("1800-12", "3", "1800-09")');
shouldBeTrue('stepMismatchFor("275760-09", "3", "275760-08")');
shouldBeFalse('stepMismatchFor("275760-09", "2", "275760-09")');
debug('Implicit step base');
shouldBeTrue('stepMismatchFor("1970-02", "2", null)');
shouldBeFalse('stepMismatchFor("1970-03", "2", null)');
debug('Fractional step values');
shouldBeFalse('stepMismatchFor("2010-03", "0.1", "2010-02")');
shouldBeFalse('stepMismatchFor("2010-03", "1.1", "2010-02")');
shouldBeTrue('stepMismatchFor("2010-03", "1.9", "2010-02")');
debug('Invalid or no step values');
shouldBeFalse('stepMismatchFor("2010-03", null, "2010-02")');
shouldBeFalse('stepMismatchFor("2010-03", "-1", "2010-02")');
shouldBeFalse('stepMismatchFor("2010-03", "foo", "2010-02")');
debug('Special step value');
shouldBeFalse('stepMismatchFor("2010-03", "any", "2010-02")');

debug('');
debug('Time type');
input.type = 'time';
debug('Empty values');
shouldBeFalse('stepMismatchFor("", null, null)');
shouldBeFalse('stepMismatchFor("", "2", "12:34:56")');
debug('Normal step values');
shouldBeFalse('stepMismatchFor("12:34:55", "1", "12:34:56")');
shouldBeTrue('stepMismatchFor("12:34:55.001", "1", "12:34:56")');
shouldBeFalse('stepMismatchFor("12:34:56.001", "0.001", "12:34:56")');
shouldBeTrue('stepMismatchFor("12:34:55", "0.333", "12:34:56")');
shouldBeFalse('stepMismatchFor("12:34:55.001", "0.333", "12:34:56")');
shouldBeFalse('stepMismatchFor("12:34", "86400", "12:34")');
shouldBeTrue('stepMismatchFor("23:59:59.998", "86399.999", "00:00")');
shouldBeFalse('stepMismatchFor("23:59:59.999", "86399.999", "00:00")');
debug('Implicit step base');
shouldBeFalse('stepMismatchFor("12:34", "120", null)');
shouldBeTrue('stepMismatchFor("12:35", "120", null)');
debug('Small step values');
shouldBeFalse('stepMismatchFor("12:34:55.001", "0.0003", "12:34.55.000")');
shouldBeTrue('stepMismatchFor("12:34:55.001", "0.0019", "12:34.55.000")');
shouldBeFalse('stepMismatchFor("12:34:55.004", "0.0019", "12:34.55.000")');
debug('Invalid or no step values');
shouldBeFalse('stepMismatchFor("12:34", null, "12:34")');
shouldBeTrue('stepMismatchFor("12:34:56", null, "12:34")');
shouldBeFalse('stepMismatchFor("12:34", "-1", "12:34")');
shouldBeFalse('stepMismatchFor("12:34", "foo", "12:34")');
debug('Special step value');
shouldBeFalse('stepMismatchFor("12:35", "any", "12:34")');

debug('');
debug('Week type');
input.type = 'week';
debug('Empty values');
shouldBeFalse('stepMismatchFor("", null, null)');
shouldBeFalse('stepMismatchFor("", "2", "1970-W02")');
debug('Normal step values');
shouldBeTrue('stepMismatchFor("2010-W03", "2", "2010-W02")');
shouldBeFalse('stepMismatchFor("2010-W02", "2", "2010-W02")');
shouldBeFalse('stepMismatchFor("2010-W04", "2", "2010-W02")');
shouldBeTrue('stepMismatchFor("1800-W11", "3", "1800-W09")');
shouldBeFalse('stepMismatchFor("1800-W09", "3", "1800-W09")');
shouldBeFalse('stepMismatchFor("1800-W12", "3", "1800-W09")');
shouldBeTrue('stepMismatchFor("275760-W35", "3", "275760-W33")');
shouldBeFalse('stepMismatchFor("275760-W35", "2", "275760-W33")');
debug('Implicit step base');
shouldBeFalse('stepMismatchFor("1970-W01", "2", null)');
shouldBeTrue('stepMismatchFor("1970-W02", "2", null)');
shouldBeFalse('stepMismatchFor("1970-W03", "2", null)');
shouldBeTrue('stepMismatchFor("1970-W04", "2", null)');
debug('Fractional step values');
shouldBeFalse('stepMismatchFor("2010-W03", "0.1", "2010-W02")');
shouldBeFalse('stepMismatchFor("2010-W03", "1.1", "2010-W02")');
shouldBeTrue('stepMismatchFor("2010-W03", "1.9", "2010-W02")');
debug('Invalid or no step values');
shouldBeFalse('stepMismatchFor("2010-W03", null, "2010-W02")');
shouldBeFalse('stepMismatchFor("2010-W03", "-1", "2010-W02")');
shouldBeFalse('stepMismatchFor("2010-W03", "foo", "2010-W02")');
debug('Special step value');
shouldBeFalse('stepMismatchFor("2010-W03", "any", "2010-W02")');

var successfullyParsed = true;
