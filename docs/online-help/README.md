# Online User Guide

The accepted manual in `resources/help/` is the common source for Qt's offline
Help and the static online edition. `topics.json` supplies all 52 topics and
seven groups. `help.qrc` supplies physical image paths, including aliases;
the generator copies the same 102 registered image resources. Article prose,
anchors, captions and screenshots are not independently maintained here.

## Build and preview locally

Requires Python 3.11 or newer, with no third-party Python or JavaScript packages:

```sh
python -B scripts/build-online-help.py
python -m http.server 8765 --bind 127.0.0.1 --directory build/online-help
```

Open `http://127.0.0.1:8765/`. Stop the server with Ctrl+C.
The generated `build/online-help/` directory is ignored. To exercise the exact
future project URL prefix instead:

```sh
python -B scripts/build-online-help.py --output build/preview/RayNmeaSimulator
python -m http.server 8765 --bind 127.0.0.1 --directory build/preview
```

Open `http://127.0.0.1:8765/RayNmeaSimulator/`. The static HTML can also be opened
from disk: search data is a local JavaScript file, not a network fetch. Browsers
without JavaScript can still navigate every article and open full-size images.

## Architecture and behavior

BrickSuite's existing README/About and shared `resources/help` → static Pages
workflow are the primary references (reviewed revision
`1e2b8f870200b634806ac37dc3629049162a7915`). This implementation adds a responsive
Darkeum-style shell to RayNmeaSimulator's own instructions; it does not copy
BrickSuite product content or change BrickSuite's license/source.

- Real, separate topic URLs preserve normal browser Back/Forward, anchors and bookmarks.
- Search is local, case-insensitive literal substring matching over titles and
  complete article text/captions. Arrow keys select results; Enter opens one;
  Escape clears search. Ctrl/Cmd+K focuses search. On small screens use Contents.
- Image links work without scripts. With scripts, the image dialog provides Fit,
  100%, zoom, scrolling, keyboard focus and Escape/Close return to the article.
- CSS/fonts/scripts are local; there is no analytics, CDN, tracking or service worker.
- `.nojekyll` supports serving the output as ordinary GitHub Pages static files.
- `site-manifest.json` records source article hashes and every generated file hash.
  The builder validates resource paths and article links/anchors before writing.
  It refuses output outside a child of `build/`, symlinks and unrelated output files.

Run `python -B tests/online_help_tests.py`, or the normal focused CTest
`online_help_tests`. Tests use the actual manual and disposable negative fixtures.
`.github/workflows/check-help.yml` builds/tests only: no Pages environment,
write permission, site artifact upload or deployment action is included.

## Publication boundary

The intended address is <https://rfstateside.github.io/RayNmeaSimulator/>.
**It is not published.** Pages configuration and deployment require separate owner
approval. After that approval, GitHub's Pages job can consume the generator's
output using the same configure/upload/deploy pattern as BrickSuite. No separate
copy of the manual should be added. Review distribution readiness in
[the maintained audit](../../PUBLIC_RELEASE_AUDIT.md) before publication.
