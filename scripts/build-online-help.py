#!/usr/bin/env python3
# Copyright (C) 2026 RF StateSide LLC. SPDX-License-Identifier: AGPL-3.0-or-later
"""Build static Help from the same catalog/articles/images embedded by Qt.
BrickSuite's shared resources/help -> static Pages architecture is the reference.
No dependency downloads, site publication or owner settings access.
"""
import argparse
import hashlib
import html
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import shutil
import xml.etree.ElementTree as ET
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
PUBLIC_URL = "https://rfstateside.github.io/RayNmeaSimulator/"

class Article(HTMLParser):
    def __init__(self, content):
        super().__init__(convert_charrefs=True)
        self.text, self.links, self.ids, self.headings = [], [], set(), []
        self.heading = None
        self.feed(content)
    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if a.get('id'):
            if a['id'] in self.ids:
                raise ValueError('Duplicate article anchor: ' + a['id'])
            self.ids.add(a['id'])
        if tag == 'a' and a.get('href'):
            self.links.append(a['href'])
        if tag == 'img':
            if not a.get('alt'):
                raise ValueError('Image without alternative text')
            self.links.append(a.get('src', ''))
        if tag == 'h2' and a.get('id'):
            self.heading = [a['id'], '']
    def handle_data(self, text):
        self.text.append(text)
        if self.heading is not None:
            self.heading[1] += text
    def handle_endtag(self, tag):
        self.text.append(' ')
        if tag == 'h2' and self.heading is not None:
            self.headings.append(self.heading)
            self.heading = None
    def plain(self):
        return re.sub(r'\s+', ' ', ''.join(self.text)).strip()

def image_paths(root):
    source = root / 'resources/help'
    result = {}
    for group in ET.parse(source / 'help.qrc').getroot().findall('qresource'):
        if group.get('prefix') != '/help':
            continue
        for file in group.findall('file'):
            alias = file.get('alias', file.text)
            if alias.startswith('images/'):
                path = (source / file.text).resolve()
                if not path.is_relative_to(root.resolve()) or not path.is_file():
                    raise ValueError('Invalid Qt image resource: ' + alias)
                result[alias] = path
    return result

def load_catalog(root):
    source = root / 'resources/help'
    catalog = json.loads((source / 'topics.json').read_text(encoding='utf-8'))
    topics, articles = [], {}
    for group in catalog['groups']:
        for topic in group['topics']:
            if not re.fullmatch(r'[a-z][a-z0-9-]*', topic['id']) or topic['file'] != topic['id'] + '.html':
                raise ValueError('Invalid topic identity')
            topic = dict(topic, group=group['title'])
            if topic['id'] in articles:
                raise ValueError('Duplicate topic')
            text = (source / topic['file']).read_text(encoding='utf-8')
            match = re.search(r'<body[^>]*>(.*?)</body>', text, re.S | re.I)
            if not match:
                raise ValueError('Missing article body: ' + topic['id'])
            body = match[1].strip()
            if re.search(r'<(?:script|iframe)\b', body, re.I):
                raise ValueError('Executable article markup is unsupported')
            articles[topic['id']] = (body, Article(body))
            topics.append(topic)
    if len(topics) != 52:
        raise ValueError('Expected the accepted 52-topic manual')
    images = set(catalog['images'])
    registered = image_paths(root)
    for image in images:
        path = registered.get(image, source / image)
        if not image.startswith('images/') or '..' in Path(image).parts or path.is_symlink() or not path.is_file() or image not in registered:
            raise ValueError('Invalid catalog image: ' + image)
    by_file = {t['file']: articles[t['id']][1] for t in topics}
    for topic in topics:
        for link in articles[topic['id']][1].links:
            url = urlsplit(link)
            if url.scheme in ('https', 'http', 'mailto'):
                continue
            if url.scheme or url.netloc or url.query or chr(92) in link or '..' in Path(unquote(url.path)).parts:
                raise ValueError('Unsupported article link: ' + link)
            path = unquote(url.path) or topic['file']
            if path in images:
                continue
            if path not in by_file or (url.fragment and unquote(url.fragment) not in by_file[path].ids):
                raise ValueError('Unresolved article link: ' + link)
    return catalog, topics, articles

def build(root, output):
    root, output = Path(root).resolve(), Path(output).absolute()
    # Reserve a child of ignored build/. Never remove an arbitrary output directory.
    if not output.resolve().is_relative_to(root / 'build') or output.resolve() == root / 'build':
        raise ValueError('Output must be a child directory of the project build/ directory')
    for part in [output, *output.parents]:
        if part.is_symlink():
            raise ValueError('Symlink output is unsupported')
    catalog, topics, articles = load_catalog(root)
    expected = {'index.html', 'assets/site.css', 'assets/site.js', 'search-index.js', '.nojekyll', 'site-manifest.json'}
    expected.update(t['file'] for t in topics)
    expected.update(catalog['images'])
    existing = {p.relative_to(output).as_posix() for p in output.rglob('*') if p.is_file()}
    if existing - expected:
        raise ValueError('Output contains unrelated files; choose an empty build/ child')
    output.mkdir(parents=True, exist_ok=True)
    esc = html.escape
    def nav(current):
        result = []
        for group in catalog['groups']:
            links = ''.join('<a href="'+t['file']+'"'+(' aria-current="page"' if t['id']==current else '')+'>'+esc(t['title'])+'</a>' for t in group['topics'])
            result.append('<section class="nav-group"><h2>'+esc(group['title'])+'</h2>'+links+'</section>')
        return ''.join(result)
    def page(title, name, current, body):
        return f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>{esc(title)} · RayNmeaSimulator User Guide</title>
<meta name="description" content="RayNmeaSimulator User Guide: marine simulation, Viewer RX, connections, routes and reference.">
<link rel="canonical" href="{PUBLIC_URL}{name}"><link rel="icon" type="image/png" href="images/boat.png"><link rel="stylesheet" href="assets/site.css">
<script src="search-index.js" defer></script><script src="assets/site.js" defer></script></head>
<body><a class="skip" href="#main">Skip to content</a>
<header class="site-header"><a class="brand" href="index.html"><span class="brand-mark" aria-hidden="true"><img src="images/boat.png" alt=""></span><span>RayNmeaSimulator<small>USER GUIDE · v0.1.0</small></span></a>
<nav aria-label="Project"><a href="https://www.rfstateside.com/">RF StateSide</a><a href="https://github.com/RFStateSide/RayNmeaSimulator">Source code</a></nav>
<button id="contents-toggle" aria-controls="site-contents" aria-expanded="false" hidden>Contents</button></header>
<div class="shell"><aside id="site-contents" aria-label="User Guide contents"><a class="guide-home" href="index.html">Guide home</a>
<form role="search" id="search-form"><label for="site-search">Search all 52 topics</label><div class="search-input"><input id="site-search" type="search" placeholder="Try Autopilot or PGN…" autocomplete="off" aria-controls="site-results"><button type="submit" aria-label="Search">↵</button></div><small>Full text search · Ctrl/Cmd+K</small></form>
<p id="search-status" role="status" aria-live="polite" hidden></p><div id="site-results" hidden></div><nav id="topic-navigation" aria-label="Topics">{nav(current)}</nav></aside>
<main id="main" tabindex="-1">{body}<footer>Copyright © 2024–2026 RF StateSide LLC · <a href="https://github.com/RFStateSide/RayNmeaSimulator/blob/master/LICENSE">AGPL-3.0-or-later</a><br>Public Engineering Reference — AS-IS. The same manual is included in the application.</footer></main></div>
<dialog id="manual-image-dialog" aria-labelledby="image-title"><div class="image-toolbar"><strong id="image-title">Image</strong><button data-zoom="fit">Fit</button><button data-zoom="actual">100%</button><button data-zoom="out" aria-label="Zoom out">−</button><button data-zoom="in" aria-label="Zoom in">+</button><button id="image-close" aria-label="Close image">Close · Esc</button></div><div id="image-viewport" tabindex="0"><img id="large-image" alt=""></div><p id="image-caption"></p></dialog></body></html>
'''
    cards = []
    for group in catalog['groups']:
        links = ''.join('<li><a href="'+t['file']+'">'+esc(t['title'])+'</a></li>' for t in group['topics'])
        cards.append('<section class="topic-card"><h2>'+esc(group['title'])+'</h2><ul>'+links+'</ul></section>')
    home = '''<div class="eyebrow">SIMULATE · INSPECT · UNDERSTAND</div><h1>Put your virtual vessel<br>to work.</h1><p class="lead">The RayNmeaSimulator User Guide. Explore marine data in Viewer RX, create repeatable simulations in Generate, and connect deliberately when your test setup is ready.</p><div class="hero-actions"><a class="primary" href="quick-start.html">Start with Quick Start →</a><a href="viewer-generate.html">Viewer or Generate?</a></div><a class="hero-image" href="images/main-window/workspace-overview.png"><img src="images/main-window/workspace-overview.png" alt="Navigation instruments, Property Editor and Object Tree in the Darkeum workspace."></a><p class="image-note">Darkeum workspace · Select any screenshot to enlarge it.</p><section class="guide-intro"><h2>One manual. Every workflow.</h2><p>Browse 52 topics, search the complete text, or open a reference directly. Start locally with Sending stopped; loading a Template or Default Layout never authorizes external output.</p><div class="reference-links"><a href="nmea2000-pgns.html">48 NMEA2000 PGNs ↗</a><a href="nmea0183-sentences.html">18 NMEA0183 formatters ↗</a><a href="enable-dispatch-sending.html">Sending controls ↗</a></div></section><div class="topic-grid">''' + ''.join(cards) + '</div>'
    (output/'index.html').write_text(page('Home', 'index.html', None, home), encoding='utf-8', newline='\n')
    for i, topic in enumerate(topics):
        body, parsed = articles[topic['id']]
        toc = ''.join('<a href="#'+esc(anchor)+'">'+esc(label)+'</a>' for anchor,label in parsed.headings)
        body = re.sub(r'(<table\b.*?</table>)', r'<div class="table-scroll" tabindex="0" aria-label="Scrollable reference table">\1</div>', body, flags=re.S|re.I)
        prev = '<a href="'+topics[i-1]['file']+'">← '+esc(topics[i-1]['title'])+'</a>' if i else '<a href="index.html">← Guide home</a>'
        following = '<a href="'+topics[i+1]['file']+'">'+esc(topics[i+1]['title'])+' →</a>' if i+1<len(topics) else '<a href="index.html">Guide home →</a>'
        content = '<div class="eyebrow">'+esc(topic['group'])+'</div><details class="on-this-page"><summary>On this page</summary><nav aria-label="On this page">'+toc+'</nav></details><article>'+body+'</article><nav class="page-turn" aria-label="Previous and next topic">'+prev+following+'</nav>'
        (output/topic['file']).write_text(page(topic['title'], topic['file'], topic['id'], content), encoding='utf-8', newline='\n')
    registered = image_paths(root)
    for image in catalog['images']:
        target=output/image; target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(registered[image],target)
    (output/'assets').mkdir(exist_ok=True)
    for asset in ['site.css','site.js']:
        shutil.copyfile(root/'docs/online-help/assets'/asset,output/'assets'/asset)
    index=[dict(id=t['id'],title=t['title'],group=t['group'],file=t['file'],text=articles[t['id']][1].plain()) for t in topics]
    (output/'search-index.js').write_text('window.RAYNMEA_HELP_INDEX = '+json.dumps(index,ensure_ascii=False).replace('<',chr(92)+'u003c')+';'+chr(10),encoding='utf-8',newline=chr(10))
    (output/'.nojekyll').write_text('',encoding='utf-8')
    manifest={'version':1,'base_url':PUBLIC_URL,'topics':len(topics),'images':len(catalog['images']),'source_articles':{t['file']:hashlib.sha256((root/'resources/help'/t['file']).read_bytes()).hexdigest() for t in topics},'files':{name:hashlib.sha256((output/name).read_bytes()).hexdigest() for name in sorted(expected-{'site-manifest.json'})}}
    (output/'site-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
    return manifest

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/online-help')
    args=parser.parse_args()
    try:
        result=build(ROOT,args.output)
    except (ValueError,KeyError,OSError) as error:
        parser.exit(1,str(error)+'\n')
    print(json.dumps({'output':str(args.output.resolve()),'topics':result['topics'],'images':result['images'],'published':False}))
if __name__=='__main__':main()
