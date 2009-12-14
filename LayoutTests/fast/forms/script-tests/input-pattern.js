description('Test the behavior of pattern attribute and pattern DOM property.');

var input = document.createElement('input');
input.type = 'text';

// No pattern attribute.
shouldBe('input.pattern', '""');

// Set a string value.
input.pattern = 'foo';
shouldBe('input.getAttribute("pattern")', '"foo"');
input.setAttribute('pattern', 'bar');
shouldBe('input.pattern', '"bar"');

// Null.
input.pattern = null;
shouldBe('input.pattern', '""');
shouldBe('input.getAttribute("pattern")', 'null');
input.setAttribute('pattern', null);
shouldBe('input.pattern', '"null"');

// Undefined.
input.pattern = undefined;
shouldBe('input.pattern', '"undefined"');
shouldBe('input.getAttribute("pattern")', '"undefined"');
input.setAttribute('pattern', undefined);
shouldBe('input.pattern', '"undefined"');

// Non-string.
input.pattern = 256;
shouldBe('input.pattern', '"256"');
shouldBe('input.getAttribute("pattern")', '"256"');
input.setAttribute('pattern', 256);
shouldBe('input.pattern', '"256"');

var successfullyParsed = true;
