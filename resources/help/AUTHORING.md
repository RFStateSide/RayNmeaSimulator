# Help authoring contract

M2.51B supplied the original 49 topics. M2.55 adds two protocol reference topics
(51 total) while retaining all original IDs.
Maintain the HTML articles directly; ignored authoring scripts are evidence, not
build prerequisites. The catalog and Qt resource file contain the complete manual.

## Catalog and articles

Keep topic IDs stable in `topics.json`. Groups are ordered; every topic has one
unique ID, title and matching `<id>.html` resource. Register each article explicitly
in `help.qrc`; do not rely on development paths or untracked files. F1 context hints
use the inherited `rayHelpTopic` widget property. The main entry defaults to Home.

Use Qt rich-text HTML: headings with stable `id` anchors, paragraphs, ordered steps,
lists, inline `code`, `pre`, tables, links and images. UTF-8 is required. This is
QTextBrowser, not a browser engine: no JavaScript, web fonts, video, CSS flex/grid,
or arbitrary embedded widgets. Keep tables narrow or split wide reference tables.
Search indexes article plain text and titles locally, including steps and captions.

## Illustrated procedures

Document real supported workflows step by step with many relevant
screenshots, explanatory captions, annotated illustrations and cross-topic links.
Organize local assets under `images/<topic-id>/` using descriptive ASCII names.
Register each image both in the catalog `images` array and in `help.qrc`. The
allowlist prevents unrelated private resources and local files being read by Help.
Prefer PNG screenshots and rasterized annotations with legible text at native DPI.
Retain artwork provenance and scrub credentials/vessel-private data before inclusion.

Darkeum is the owner-selected primary screenshot theme. Select it explicitly in a
disposable profile; do not change application defaults or overwrite user themes.
Other themes belong only in theme instructions/comparisons or relevant exceptions.
`images/capture-provenance.json` records each native capture, dimensions, synthetic
context and SHA-256. It is an authoring record, not a runtime resource. Preserve
the existing third-party notices for the application UI depicted in screenshots.

The explicit `help_manual_capture` test helper renders real Qt windows/dialogs and
is excluded from normal builds/CTest. Set `RAY_HELP_CAPTURE_ROOT` to an ignored
output directory and run it on the native desktop. Its temporary settings and
credential identities, synthetic routes and memory-only serial sink keep owner
data and physical transports separate. Linux native captures additionally use a
disposable HOME/config and inaccessible D-Bus endpoints. Review every resulting
image before copying it into resources; capture success is not privacy proof.
`RAY_HELP_CAPTURE_ONLY=socketcan` captures only the Linux-specific Setup page.
`RAY_HELP_CAPTURE_ONLY=m255` captures empty/populated Viewer views and the real
connected controller, failing explicitly if SDL reports None Found. Use the native
desktop, with a physical controller attached. This subset opens no native file
dialogs, so it skips the full-manual helper's process-local registry redirection:
SDL must be able to read the real Windows device mapping configuration. It still
uses disposable AppSettings and all marine interfaces are disabled. The controller
name is actual hardware detection; vessel values and RX messages are synthetic.
`m255-viewer` captures only the two Viewer examples. Optional
`RAY_HELP_CAPTURE_PREVIEW=1` holds the controller view for two minutes.
Screenshots are not hardware qualification. Do not click provider, external-link,
discovery or request controls merely to produce illustrations.

Each image can be linked to its own resource for click-to-enlarge:

```html
<p><a href="images/route-import/choose-file.png">
  <img src="images/route-import/choose-file.png"
       alt="Select Route File with a disposable GPX selected" />
</a><br /><i>Figure 1. Select the route file; selection alone does not engage Autopilot.</i></p>
```

The inline renderer preserves aspect ratio, fits the reading width and does not
upscale beyond intrinsic/explicit width. Multiple images per article are supported.
The separate image window provides Fit, 100%, Zoom In/Out, scrolling and Escape.
Its keyboard shortcuts are Ctrl+0, Ctrl+1 and the platform Zoom In/Out bindings.
Annotated illustrations are ordinary local images; captions/alt text also explain
their meaning. Keep engineering data in searchable text/tables, not images alone.

Use relative article links such as `route-export.html#top`. Only registered topics
and images are accessible. External links must be explicit HTTPS links; they open
the system browser only on activation. No remote images, file URLs, scripts,
embedded credentials or automatic network content. Style colors come from the
active application palette/QSS; avoid fixed light-only body/text colors.

## Qualification for each completed topic

Verify the actual workflow against current capabilities, direction/transport
restrictions and explicit Sending/route authorization. Check internal anchors,
image registration, captions, search terms and readability in six built-in themes,
custom QSS, compact windows and high DPI. Use synthetic/private-redacted fixtures;
serializer round trips do not establish physical chartplotter interoperability.
The Phase 1 logo is an image-path fixture, not an instructional screenshot.

## User-facing editorial voice

Write for the person operating the application: explain controls, steps, results
and real functional limits. Keep audit/qualification history, test-fixture details,
owner approval records and screenshot-capture methods out of article text, image
alt text, captions and Contents labels. Retain that material in provenance records
or ignored build evidence. Retain explicit Sending, Stop/F12, connection,
experimental-feature and data-validity instructions. Keep topic/anchor IDs stable.
Owner-observed live integrations belong in private evidence separately from
software tests; Help describes the practical compatible workflow without inventing
unreported endpoint details. Review all rendered pages as well as source text.
The Help test's optional RAY_HELP_EDITORIAL_RENDER directory receives native
Darkeum page views without altering the retained screenshot assets.
