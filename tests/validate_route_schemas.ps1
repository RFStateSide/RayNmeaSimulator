# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
# Offline XSD 1.0 qualification. Obtain official schemas separately; no runtime downloads.
# Usage: pwsh -File tests/validate_route_schemas.ps1 -SchemaDirectory <local-cache>
# Cache: gpx10.xsd, gpx11.xsd, ogckml22.xsd, atom-author-link.xsd, xAL.xsd.
param([Parameter(Mandatory=$true)][string]$SchemaDirectory,
      [string]$ExportDirectory = '')
$ErrorActionPreference = 'Stop'
$fixtureDirectory = Join-Path $PSScriptRoot 'fixtures/routes'
if ($ExportDirectory) { $fixtureDirectory = $ExportDirectory }
function Test-RouteSchema([string]$Schema, [string]$Fixture, [bool]$Kml = $false) {
    $schemas = [System.Xml.Schema.XmlSchemaSet]::new()
    $schemas.XmlResolver = $null
    # GPX 1.0's foreign-element wildcard is strict (1.1 uses lax).
    if ($Schema -eq 'gpx10.xsd') {
        [void]$schemas.Add('urn:raynmea:fixture', (Join-Path $fixtureDirectory 'vendor.xsd'))
    }
    if ($Kml) {
        [void]$schemas.Add('http://www.w3.org/2005/Atom', (Join-Path $SchemaDirectory 'atom-author-link.xsd'))
        [void]$schemas.Add('urn:oasis:names:tc:ciq:xsdschema:xAL:2.0', (Join-Path $SchemaDirectory 'xAL.xsd'))
    }
    $targetNamespace = if ($Kml) { 'http://www.opengis.net/kml/2.2' } elseif ($Schema -eq 'gpx10.xsd') { 'http://www.topografix.com/GPX/1/0' } else { 'http://www.topografix.com/GPX/1/1' }
    [void]$schemas.Add($targetNamespace, (Join-Path $SchemaDirectory $Schema))
    $schemas.Compile()
    $settings = [System.Xml.XmlReaderSettings]::new()
    $settings.XmlResolver = $null
    $settings.DtdProcessing = [System.Xml.DtdProcessing]::Prohibit
    $settings.ValidationType = [System.Xml.ValidationType]::Schema
    $settings.Schemas = $schemas
    # Deliberately do not process instance schemaLocation or load vendor schemas.
    $reader = [System.Xml.XmlReader]::Create((Join-Path $fixtureDirectory $Fixture), $settings)
    try { while ($reader.Read()) {} } finally { $reader.Dispose() }
    Write-Output "OFFLINE_SCHEMA_PASS $Fixture ($Schema)"
}
if ($ExportDirectory) {
    # route_export_tests writes these only when RAYNMEA_EXPORT_SCHEMA_DIR is set.
    Test-RouteSchema 'gpx11.xsd' 'export.gpx'
    Test-RouteSchema 'ogckml22.xsd' 'export.kml' $true
    return
}
Test-RouteSchema 'gpx10.xsd' 'gpx10-route.gpx'
Test-RouteSchema 'gpx11.xsd' 'gpx11-track.gpx'
Test-RouteSchema 'ogckml22.xsd' 'kml22-linestring.kml' $true
# KML 2.3 uses XSD 1.1 assertions/open content; System.Xml supports XSD 1.0.
# Do not remove assertions or claim schema validation for KML 2.3 / gx extensions.
