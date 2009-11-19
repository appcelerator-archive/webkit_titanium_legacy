description('This test aims to check for typeMismatch flag with type=date input fields');
var i = document.createElement('input');
i.type = 'date';

function check(value, mismatchExpected)
{
    i.value = value;
    var actual = i.validity.typeMismatch;
    var didPass = actual == mismatchExpected;
    var resultText = '"' + value + '" is ' + (didPass ? 'a correct ' : 'an incorrect ') + (actual ? 'invalid' : 'valid') + ' date string.';
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
shouldBeValid('2009-09-07');
shouldBeValid('9999-12-31');
shouldBeValid('2147483647-12-31');
shouldBeValid('1582-10-15');
shouldBeValid('2009-01-31');
shouldBeValid('2009-02-28');
shouldBeValid('2009-03-31');
shouldBeValid('2009-04-30');
shouldBeValid('2009-05-31');
shouldBeValid('2009-06-30');
shouldBeValid('2009-07-31');
shouldBeValid('2009-08-31');
shouldBeValid('2009-09-30');
shouldBeValid('2009-10-31');
shouldBeValid('2009-11-30');
shouldBeValid('2009-12-31');
shouldBeValid('2008-02-29');
shouldBeValid('2000-02-29');

// Invalid values
shouldBeInvalid(' 2009-09-07 ');
shouldBeInvalid('a');
shouldBeInvalid('-1-09-07');
shouldBeInvalid('1581-09-07');
shouldBeInvalid('1582-10-14');
shouldBeInvalid('2147483648-12-31');
shouldBeInvalid('2009/09/07');
shouldBeInvalid('20090907');
shouldBeInvalid('2009--2-07');
shouldBeInvalid('2009-00-07');
shouldBeInvalid('2009-13-07');
shouldBeInvalid('1582-09-30');
shouldBeInvalid('2009-xx-30');
shouldBeInvalid('2009-01-32');
shouldBeInvalid('2009-02-29');
shouldBeInvalid('2009-03-32');
shouldBeInvalid('2009-04-31');
shouldBeInvalid('2009-05-32');
shouldBeInvalid('2009-06-31');
shouldBeInvalid('2009-07-32');
shouldBeInvalid('2009-08-32');
shouldBeInvalid('2009-09-31');
shouldBeInvalid('2009-10-32');
shouldBeInvalid('2009-11-31');
shouldBeInvalid('2009-12-32');
shouldBeInvalid('2100-02-29');
shouldBeInvalid('2009-09-00');
shouldBeInvalid('2009-09-');
shouldBeInvalid('2009-09-7');
shouldBeInvalid('2009-09-001');
shouldBeInvalid('2009-09-xx');
shouldBeInvalid('2009-09');
shouldBeInvalid('2009');

var successfullyParsed = true;
