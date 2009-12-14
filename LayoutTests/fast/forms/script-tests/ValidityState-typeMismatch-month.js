description('This test aims to check for typeMismatch flag with type=month input fields');
var i = document.createElement('input');
i.type = 'month';

function check(value, mismatchExpected)
{
    i.value = value;
    var actual = i.validity.typeMismatch;
    var didPass = actual == mismatchExpected;
    var resultText = '"' + value + '" is ' + (didPass ? 'a correct ' : 'an incorrect ') + (actual ? 'invalid' : 'valid') + ' month string.';
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
shouldBeValid('1582-10');
shouldBeValid('9999-12');
shouldBeValid('2147483647-12');
shouldBeValid('2009-01');
shouldBeValid('2009-02');
shouldBeValid('2009-03');
shouldBeValid('2009-04');
shouldBeValid('2009-05');
shouldBeValid('2009-06');
shouldBeValid('2009-07');
shouldBeValid('2009-08');
shouldBeValid('2009-09');
shouldBeValid('2009-10');
shouldBeValid('2009-11');
shouldBeValid('2009-12');

// Invalid values
shouldBeInvalid(' 2009-09 ');
shouldBeInvalid('a');
shouldBeInvalid('-1-09');
shouldBeInvalid('1581-10');
shouldBeInvalid('1582-09');
shouldBeInvalid('2147483648-12');
shouldBeInvalid('2009/09');
shouldBeInvalid('200909');
shouldBeInvalid('2009--2');
shouldBeInvalid('2009-00');
shouldBeInvalid('2009-13');
shouldBeInvalid('2009-');
shouldBeInvalid('2009-9');
shouldBeInvalid('2009-009');
shouldBeInvalid('2009-xx');
shouldBeInvalid('2009');

var successfullyParsed = true;
