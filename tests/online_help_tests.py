# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Actual 52-topic manual regressions; no owner settings, Templates or Layouts."""
import hashlib
from html.parser import HTMLParser
import importlib.util
import json
from pathlib import Path
import re
import shutil
import tempfile
import unittest
from urllib.parse import unquote, urlsplit

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('online_help',ROOT/'scripts/build-online-help.py')
site=importlib.util.module_from_spec(spec);spec.loader.exec_module(site)
class Links(HTMLParser):
    def __init__(self,text):
        super().__init__();self.links=[];self.ids=set();self.feed(text)
    def handle_starttag(self,tag,attrs):
        d=dict(attrs)
        if d.get('id'):self.ids.add(d['id'])
        for key in ('href','src'):
            if d.get(key):self.links.append(d[key])

class OnlineHelpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT/'build').mkdir(exist_ok=True)
        cls.temp=tempfile.TemporaryDirectory(prefix='online-help-test-',dir=ROOT/'build')
        cls.base=Path(cls.temp.name).resolve();assert cls.base.is_relative_to(ROOT/'build')
        cls.output=cls.base/'site';cls.manifest=site.build(ROOT,cls.output)
        cls.catalog,cls.topics,cls.articles=site.load_catalog(ROOT)
    @classmethod
    def tearDownClass(cls):
        assert cls.base.is_relative_to(ROOT/'build');cls.temp.cleanup()
    def test_actual_inventory_and_shared_article_text(self):
        self.assertEqual(len(self.topics),52);self.assertEqual(len(self.catalog['groups']),7)
        for t in self.topics:
            text=(self.output/t['file']).read_text(encoding='utf-8')
            body=re.search(r'<article>(.*?)</article>',text,re.S)[1]
            self.assertEqual(site.Article(body).plain(),self.articles[t['id']][1].plain(),t['id'])
            self.assertIn('aria-current="page"',text)
    def test_owner_approved_safety_in_actual_shared_sources(self):
        # Digests pin the exact owner wording, list order and bold ranges without
        # maintaining a second manual for the online edition.
        source=(ROOT/'resources/help/engineering-reference.html').read_text(encoding='utf-8')
        block=re.search(r'<h2 id="safety">Safety</h2>(.*?)(?=<h2)',source,re.S)[0]
        class Words(HTMLParser):
            def __init__(self,text):
                super().__init__(convert_charrefs=True);self.bits=[];self.feed(text)
            def handle_data(self,text):self.bits.append(text)
            def handle_endtag(self,tag):
                if tag in ('h2','p','li'):self.bits.append(' ')
            def plain(self):return re.sub(r'\s+',' ',''.join(self.bits)).strip()
        plain=Words(block).plain()
        self.assertEqual(hashlib.sha256(plain.encode()).hexdigest(),'17b5639c0d597ed54a47d45555ef7b82d94def904beb3d7ffce955a98f41c70b')
        self.assertEqual(block.count('<li>'),6)
        spans=[Words(x).plain() for x in re.findall(r'<strong>(.*?)</strong>',block,re.S)]
        self.assertEqual(len(spans),3)
        self.assertEqual(hashlib.sha256(json.dumps(spans,ensure_ascii=False,separators=(',',':')).encode()).hexdigest(),'3c756691e627b23ef8ede2c5ca336bcc18ac732828f9d76548b22f0493548dfd')
        readme=(ROOT/'README.md').read_text(encoding='utf-8')
        body=re.search(r'^## Safety\n\n(.*?)(?=^!\[|^## |\Z)',readme,re.S|re.M)[1].strip()
        self.assertEqual(hashlib.sha256(('Safety\n\n'+body).encode()).hexdigest(),'6df3423f0bfb9c657bf5bdc3f68992eda0c0b40e62cce28a401311404122dd85')
        self.assertLess(readme.index('## Safety'),readme.index('## Two ways to work'))
        generated=(self.output/'engineering-reference.html').read_text(encoding='utf-8')
        self.assertIn(block,generated)
        self.assertIn('id="limits"',source);self.assertIn('id="engineering-method"',source)
        topic=next(t for t in self.topics if t['id']=='engineering-reference')
        self.assertEqual(topic['file'],'engineering-reference.html')
        index=json.loads((self.output/'search-index.js').read_text().removeprefix('window.RAYNMEA_HELP_INDEX = ').strip().removesuffix(';'))
        entry=next(t for t in index if t['id']=='engineering-reference')
        for text in ['collision-avoidance','Already queued or transmitted data','F12 / Stop']:
            self.assertIn(text,entry['text'])
    def test_every_local_link_asset_and_anchor_resolves(self):
        pages={p.name:Links(p.read_text(encoding='utf-8')) for p in self.output.glob('*.html')}
        self.assertEqual(len(pages),53)
        for name,page in pages.items():
            for link in page.links:
                u=urlsplit(link)
                if u.scheme or u.netloc:continue
                target=unquote(u.path) or name
                self.assertTrue((self.output/target).is_file(),(name,link))
                if u.fragment:self.assertIn(unquote(u.fragment),pages[target].ids,(name,link))
    def test_images_match_actual_compiled_resource_paths(self):
        resources=site.image_paths(ROOT)
        self.assertEqual(len(self.catalog['images']),102)
        for image in self.catalog['images']:
            self.assertEqual((self.output/image).read_bytes(),resources[image].read_bytes(),image)
        self.assertEqual((self.output/'images/boat.png').read_bytes(),(ROOT/'images/RayNmeaSimulator_1024.png').read_bytes())
    def test_search_index_uses_all_actual_article_text(self):
        s=(self.output/'search-index.js').read_text(encoding='utf-8')
        index=json.loads(s.removeprefix('window.RAYNMEA_HELP_INDEX = ').strip().removesuffix(';'))
        self.assertEqual(len(index),52)
        for topic in index:self.assertEqual(topic['text'],self.articles[topic['id']][1].plain())
        self.assertTrue(any('OpenCPN' in t['text'] for t in index));self.assertTrue(any('APB' in t['text'] for t in index))
    def test_deterministic_hash_manifest(self):
        self.assertEqual(site.build(ROOT,self.output),self.manifest)
        for file,sha in self.manifest['files'].items():self.assertEqual(hashlib.sha256((self.output/file).read_bytes()).hexdigest(),sha)
    def test_output_boundary_and_unrelated_file_preservation(self):
        with self.assertRaises(ValueError):site.build(ROOT,ROOT/'resources/help')
        with self.assertRaises(ValueError):site.build(ROOT,ROOT/'build')
        other=self.base/'unrelated';other.mkdir();(other/'owner.txt').write_text('preserve',encoding='utf-8')
        with self.assertRaises(ValueError):site.build(ROOT,other)
        self.assertEqual((other/'owner.txt').read_text(),'preserve')
    def test_negative_actual_catalog_fixtures(self):
        fixture=self.base/'negative-source';(fixture/'resources').mkdir(parents=True)
        shutil.copytree(ROOT/'resources/help',fixture/'resources/help')
        (fixture/'images').mkdir();shutil.copyfile(ROOT/'images/RayNmeaSimulator_1024.png',fixture/'images/RayNmeaSimulator_1024.png')
        article=fixture/'resources/help/quick-start.html';original=article.read_text(encoding='utf-8')
        article.write_text(original.replace('</body>','<a href="introduction.html#missing-anchor">Broken</a></body>'),encoding='utf-8')
        with self.assertRaisesRegex(ValueError,'Unresolved article link'):site.load_catalog(fixture)
        article.write_text(original,encoding='utf-8');catalog=fixture/'resources/help/topics.json';d=json.loads(catalog.read_text());d['groups'][0]['topics'][0]['file']='../outside.html';catalog.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'Invalid topic'):site.load_catalog(fixture)
    def test_readme_local_links_images_and_anchors(self):
        text=(ROOT/'README.md').read_text(encoding='utf-8')
        for link in re.findall(r'\]\(([^)]+)\)',text):
            u=urlsplit(link)
            if u.scheme or u.netloc:continue
            self.assertTrue((ROOT/unquote(u.path)).is_file(),link)
        self.assertIn('https://github.com/RFStateSide/RayNmeaSimulator/releases',text)
        self.assertNotIn('not published yet',text)
        self.assertNotIn('GitHub Releases](https://github.com/',text)
    def test_ci_has_no_pages_deployment(self):
        text=(ROOT/'.github/workflows/check-help.yml').read_text()
        for token in ['pages: write','id-token: write','uses: actions/deploy-pages','uses: actions/upload-pages-artifact','uses: actions/configure-pages']:
            self.assertNotIn(token,text)
if __name__=='__main__':unittest.main(verbosity=2)
