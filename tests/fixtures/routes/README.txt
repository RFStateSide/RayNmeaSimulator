M2.25 route qualification fixtures

gpx10-route.gpx, gpx11-track.gpx, kml22-linestring.kml and kml23-track.kml
are independently constructed project fixtures, under the project's license.
Expected ordered latitude/longitude pairs for each:
  (42.125, -70.25), (42.25, -70.5).
Names, namespace prefixes, vendor metadata, unrelated pins, nested folders,
MultiGeometry and schemaLocation declarations intentionally differ.
The runtime never fetches schemaLocation, styles, links or vendor resources.

google-earth-track.kml is the unmodified XML "Example of Track" code sample:
https://developers.google.com/kml/documentation/kmlreference#gxtrack
Retrieved 2026-10-18. Source: Google KML Reference, Google Developers.
The page licenses code samples under Apache License 2.0; included in
APACHE-2.0.txt. This is a published Google Earth-compatible example, not an
independently captured export or evidence of a Google Earth runtime test.
Seven coordinate triples in document order are compared with independent
literal expected latitude/longitude values by route_import_tests.

Existing routes/gpx and routes/kml samples remain unmodified. Three existing
KML files contain both a LineString and matching Point waypoint markers.
The LineString defines geometry; matching names and per-waypoint speeds are
preserved. Point-only legacy files require explicit user confirmation because
Point collections do not intrinsically describe an ordered route in KML.

Standards and official schemas reviewed:
https://www.topografix.com/gpx/1/1/
https://www.topografix.com/gpx_manual.asp
https://www.topografix.com/gpx_for_developers.asp
https://www.topografix.com/GPX/1/0/gpx.xsd
https://www.topografix.com/GPX/1/1/gpx.xsd
https://www.ogc.org/standards/kml/
https://schemas.opengis.net/kml/2.2.0/ogckml22.xsd
https://schemas.opengis.net/kml/2.3/ogckml23.xsd
https://docs.ogc.org/is/12-007r2/12-007r2.html

tests/validate_route_schemas.ps1 performs offline GPX 1.0/1.1 and KML 2.2
fixture schema checks using separately obtained local official schemas.
No new runtime dependency or schema download is introduced. Official schema
copies and qualification logs are ignored engineering evidence, not bundled
application resources. KML 2.3 requires XSD 1.1, unavailable in System.Xml;
its schema was reviewed, and its supported spatial subset is tested by Qt.
This is ordered-route subset qualification, not full-document conformance
certification, universal exporter compatibility, or macOS/Pi runtime testing.
