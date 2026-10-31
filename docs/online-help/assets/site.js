/* Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later */
(() => {
  'use strict';
  document.body.classList.add('js');
  const contents = document.querySelector('#site-contents');
  const toggle = document.querySelector('#contents-toggle');
  toggle.hidden = false;
  toggle.addEventListener('click', () => {
    const open = contents.classList.toggle('open');
    toggle.setAttribute('aria-expanded', String(open));
  });
  const input = document.querySelector('#site-search');
  const results = document.querySelector('#site-results');
  const navigation = document.querySelector('#topic-navigation');
  const status = document.querySelector('#search-status');
  let selected = 0;
  function select(index) {
    const links = [...results.querySelectorAll('a')];
    selected = Math.max(0, Math.min(index, links.length - 1));
    links.forEach((link, i) => link.classList.toggle('selected', i === selected));
    if (links[selected]) links[selected].scrollIntoView({block: 'nearest'});
  }
  function search() {
    const query = input.value.trim().toLocaleLowerCase();
    results.replaceChildren(); selected = 0;
    results.hidden = status.hidden = !query;
    navigation.hidden = !!query;
    if (!query) return;
    const matches = window.RAYNMEA_HELP_INDEX.filter(topic => (topic.title + '\n' + topic.text).toLocaleLowerCase().includes(query));
    status.textContent = `${matches.length} ${matches.length === 1 ? 'topic' : 'topics'} found`;
    for (const topic of matches) {
      const link = document.createElement('a');
      link.href = topic.file; link.className = 'search-result';
      link.textContent = topic.title;
      const excerpt = document.createElement('small');
      const at = topic.text.toLocaleLowerCase().indexOf(query);
      const start = Math.max(0, at - 45);
      excerpt.textContent = (start ? '…' : '') + topic.text.slice(start, start + 160) + '…';
      link.append(excerpt); results.append(link);
    }
    select(0);
  }
  input.addEventListener('input', search);
  input.addEventListener('keydown', event => {
    if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
      event.preventDefault(); select(selected + (event.key === 'ArrowDown' ? 1 : -1));
    } else if (event.key === 'Escape') {
      input.value = ''; search();
    }
  });
  document.querySelector('#search-form').addEventListener('submit', event => {
    event.preventDefault();
    const link = results.querySelectorAll('a')[selected];
    if (link) window.location.assign(link.href);
  });
  document.addEventListener('keydown', event => {
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'k') {
      event.preventDefault(); contents.classList.add('open');
      toggle.setAttribute('aria-expanded', 'true'); input.focus(); input.select();
    }
  });
  const dialog = document.querySelector('#manual-image-dialog');
  const large = document.querySelector('#large-image');
  const viewport = document.querySelector('#image-viewport');
  let scale = 1, trigger;
  function zoom(mode) {
    if (mode === 'fit') scale = Math.min(1, (viewport.clientWidth - 20) / large.naturalWidth, (viewport.clientHeight - 20) / large.naturalHeight);
    if (mode === 'actual') scale = 1;
    if (mode === 'in') scale = Math.min(4, scale * 1.25);
    if (mode === 'out') scale = Math.max(.1, scale / 1.25);
    large.style.width = Math.round(large.naturalWidth * scale) + 'px';
  }
  document.querySelectorAll('main a').forEach(link => {
    const image = link.querySelector('img');
    if (!image || !link.getAttribute('href').startsWith('images/')) return;
    link.addEventListener('click', event => {
      // The original full-image link still works when dialog APIs are unavailable.
      if (!dialog.showModal) return;
      event.preventDefault(); trigger = link;
      large.onload = () => zoom('fit'); large.alt = image.alt; large.src = link.href;
      document.querySelector('#image-caption').textContent = image.alt;
      document.querySelector('#image-title').textContent = 'Screenshot';
      dialog.showModal(); if (large.complete) zoom('fit');
      document.querySelector('#image-close').focus();
    });
  });
  document.querySelectorAll('[data-zoom]').forEach(button => button.addEventListener('click', () => zoom(button.dataset.zoom)));
  document.querySelector('#image-close').addEventListener('click', () => dialog.close());
  dialog.addEventListener('close', () => { if (trigger) trigger.focus(); });
})();
