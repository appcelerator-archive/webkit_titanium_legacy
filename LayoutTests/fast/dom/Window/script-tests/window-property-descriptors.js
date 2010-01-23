description("Test Object.getOwnPropertyDescriptor on window object.");

var __skip__ = {
    "__skip__" : 1,
    "opener" : 1, // Work around DumpRenderTree bug where previous tests add window properties
    "Components" : 1, // Work around Firefox exception

    // Don't log DumpRenderTree injected objects
    "layoutTestController" : 1, // Work around http://bugs.webkit.org/show_bug.cgi?id=11373
    "GCController" : 1,
    "accessibilityController" : 1,
    "appleScriptController" : 1,
    "eventSender" : 1,
    "navigationController" : 1,
    "objCController" : 1,
    "objCPlugin" : 1,
    "objCPluginFunction" : 1,
    "plainText" : 1,
    "textInputController" : 1,
    // Ignore these properties because they do not exist in all implementations. They will be tested separately
    "WebGLRenderingContext" : 1,
    "WebGLArrayBuffer" : 1,
    "WebGLByteArray" : 1,
    "WebGLFloatArray" : 1,
    "WebGLIntArray" : 1,
    "WebGLShortArray" : 1,
    "WebGLUnsignedByteArray" : 1,
    "WebGLUnsignedIntArray" : 1,
    "WebGLUnsignedShortArray" : 1,
    // Ignore this property because it only appears in debug builds and not in release.
    "jscprint" : 1
};

var windowPropertyNames = Object.getOwnPropertyNames(window)
    .filter(function(name) { return !__skip__[name]; })
    .sort();

for (var i = 0; i < windowPropertyNames.length; ++i)
    shouldBe("typeof Object.getOwnPropertyDescriptor(window, '" + windowPropertyNames[i] + "')", "'object'");

// Properties in the prototype should not return descriptors

var protoPropertySet = {};
for (var o = window.__proto__; o; o = o.__proto__) {
    var names = Object.getOwnPropertyNames(o);
    for (var i = 0; i < names.length; ++i)
        protoPropertySet[names[i]] = true;
}

var protoPropertyNames = [];
for (var name in protoPropertySet)
    protoPropertyNames.push(name);
protoPropertyNames.sort();

for (var i = 0; i < protoPropertyNames.length; ++i)
    shouldBeUndefined("Object.getOwnPropertyDescriptor(window, '" + protoPropertyNames[i] + "')");

var successfullyParsed = true;
