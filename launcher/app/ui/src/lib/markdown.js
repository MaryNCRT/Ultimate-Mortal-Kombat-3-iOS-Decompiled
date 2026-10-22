/* A release's notes are GitHub markdown: headings, bullets (one level of
 * nesting), **bold**, `code`, *emphasis* and [links](...). Parsed here into
 * plain blocks the Notes component draws as text, never as HTML.
 *
 *   blocks: { kind: 'h' | 'p', spans } | { kind: 'ul', items }
 *   items:  { spans, sub: items }
 *   spans:  { kind: 'text' | 'b' | 'code' | 'link' | 'em', text } */

export function inlineSpans(text) {
    const re = /(\*\*[^*]+\*\*|`[^`]+`|\[[^\]]+\]\([^)]+\)|\*[^*\s][^*]*\*)/g;
    const out = [];
    let at = 0, m;
    while ((m = re.exec(text))) {
        if (m.index > at) out.push({ kind: 'text', text: text.slice(at, m.index) });
        const t = m[0];
        if (t.startsWith('**')) out.push({ kind: 'b', text: t.slice(2, -2) });
        else if (t.startsWith('`')) out.push({ kind: 'code', text: t.slice(1, -1) });
        else if (t.startsWith('[')) out.push({ kind: 'link', text: t.slice(1, t.indexOf(']')) });
        else out.push({ kind: 'em', text: t.slice(1, -1) });
        at = m.index + t.length;
    }
    if (at < text.length) out.push({ kind: 'text', text: text.slice(at) });
    return out;
}

export function parseNotes(md) {
    const blocks = [];
    let list = null;
    for (const raw of String(md || '').replace(/\r/g, '').split('\n')) {
        const line = raw.trimEnd();
        const h = line.match(/^#{1,6}\s+(.*)$/);
        const li = line.match(/^\s*[-*]\s+(.*)$/);
        if (li) {
            if (!list) { list = { kind: 'ul', items: [] }; blocks.push(list); }
            const item = { spans: inlineSpans(li[1]), sub: [] };
            const parent = /^\s{2,}/.test(line) && list.items.length ? list.items[list.items.length - 1].sub : list.items;
            parent.push(item);
            continue;
        }
        list = null;
        if (!line.trim()) continue;
        blocks.push({ kind: h ? 'h' : 'p', spans: inlineSpans(h ? h[1] : line) });
    }
    return blocks;
}
