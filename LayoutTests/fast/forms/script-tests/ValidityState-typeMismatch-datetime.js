description('This test aims to check for typeMismatch flag with type=datetime input fields');
var i = document.createElement('input');
i.type = 'datetime';

function check(value, mismatchExpected)
{
    i.value = value;
    var actual = i.validity.typeMismatch;
    var didPass = actual == mismatchExpected;
    var resultText = '"' + value + '" is ' + (didPass ? 'a correct ' : 'an incorrect ') + (actual ? 'invalid' : 'valid') + ' datetime string.';
    if (didPass)
        testPassed(resultText);
    else
        testFailed(resultText);
}

function shouldBeValid(value)
{
    check(value, false);
}

function shouldBeInvalid(value)
{
    check(value, true);
}

// Valid values
shouldBeValid('');
shouldBeValid('2009-09-07T16:49Z');
shouldBeValid('2009-09-07T16:49:31Z');
shouldBeValid('2009-09-07T16:49:31.1Z');
shouldBeValid('2009-09-07T16:49:31.12Z');
shouldBeValid('2009-09-07T16:49:31.123Z');
shouldBeValid('2009-09-07T16:49:31.1234567890Z');
shouldBeValid('2147483647-12-31T23:59:59.999Z');
shouldBeValid('1582-10-15T00:00:00.000Z');
shouldBeValid('1582-10-15T00:00:00.000+00:00');
shouldBeValid('1582-10-15T01:30+01:30');
shouldBeValid('1582-10-15T00:00+01:00');

// Invalid values
shouldBeInvalid(' 2009-09-07T16:49Z ');
shouldBeInvalid('2009-09-07T16:49z');
shouldBeInvalid('2009-09-07t16:49Z');
shouldBeInvalid('2009-09-07T16:49');
shouldBeInvalid('2009/09/07T16:49Z');
shouldBeInvalid('a');
shouldBeInvalid('-1-09-07T16:49Z');
shouldBeInvalid('2009-09-07T16:49+23');
shouldBeInvalid('2009-09-07T16:49+23:');
shouldBeInvalid('2009-09-07T16:49+24:00');
shouldBeInvalid('2009-09-07T16:49+23:60');
shouldBeInvalid('2009-09-07T16:49-23');
shouldBeInvalid('2009-09-07T16:49-23:');
shouldBeInvalid('2009-09-07T16:49-24:00');
shouldBeInvalid('2009-09-07T16:49-23:60');
shouldBeInvalid('1582-10-14T23:59:59Z');
shouldBeInvalid('2147483647-12-31T23:59-00:01');
shouldBeInvalid('2147483648-01-01T00:00:00Z');

var successfullyParsed = true;
