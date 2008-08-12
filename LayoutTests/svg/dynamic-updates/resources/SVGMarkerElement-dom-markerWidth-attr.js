// [Name] SVGMarkerElement-dom-markerWidth-attr.js
// [Expected rendering result] start & end markers are visible - and a series of PASS messages

description("Tests dynamic updates of the 'markerWidth' attribute of the SVGMarkerElement object")
createSVGTestCase();

var markerElement = createSVGElement("marker");
markerElement.setAttribute("id", "marker");
markerElement.setAttribute("viewBox", "0 0 10 10");
markerElement.setAttribute("markerWidth", "0");
markerElement.setAttribute("markerHeight", "2");
markerElement.setAttribute("refX", "5");
markerElement.setAttribute("refY", "5");
markerElement.setAttribute("markerUnits", "strokeWidth");

var markerPathElement = createSVGElement("path");
markerPathElement.setAttribute("fill", "blue");
markerPathElement.setAttribute("d", "M 5 0 L 10 10 L 0 10 Z");
markerElement.appendChild(markerPathElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(markerElement);
rootSVGElement.appendChild(defsElement);

var pathElement = createSVGElement("path");
pathElement.setAttribute("fill", "none");
pathElement.setAttribute("stroke", "green");
pathElement.setAttribute("stroke-width", "10");
pathElement.setAttribute("marker-start", "url(#marker)");
pathElement.setAttribute("marker-end", "url(#marker)");
pathElement.setAttribute("d", "M 130 135 L 180 135 L 180 185");
rootSVGElement.appendChild(pathElement);

shouldBeEqualToString("markerElement.getAttribute('markerWidth')", "0");

function executeTest() {
    markerElement.setAttribute("markerWidth", "2");
    shouldBeEqualToString("markerElement.getAttribute('markerWidth')", "2");

    completeTest();
}

startTest(pathElement, 180, 180);
