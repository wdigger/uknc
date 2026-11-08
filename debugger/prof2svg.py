#!/usr/bin/env python3

"""Turn `monitor prof save` output into a static SVG report.

The emulator writes a flat tick profile: one octal address and one tick
count per line.  This script names those addresses through addr2line,
groups them the same way as prof2cpuprofile.py, and draws a simple
self-time bar chart that can be opened in a browser, sent in a report or
looked at without VS Code.

    debugger/prof2svg.py -e hello.elf hello.prof
    debugger/prof2svg.py --by function --limit 25 -e hello.elf hello.prof

For a PPU profile pass the load base printed by ppu.gdb:

    debugger/prof2svg.py -e gfppu.ppu.elf -b 023666 ppu.prof
"""

import argparse
import html
import os
import sys

from prof2cpuprofile import group
from prof2cpuprofile import parse_number
from prof2cpuprofile import read_profile
from prof2cpuprofile import symbolize


def shorten(text, limit):
    if len(text) <= limit:
        return text
    if limit <= 1:
        return text[:limit]
    return text[:limit - 3] + '...'


def source_name(path, line, base_dir):
    if not path:
        return ''
    shown = path
    if os.path.isabs(path):
        try:
            shown = os.path.relpath(path, base_dir)
        except ValueError:
            shown = path
    if line > 0:
        shown += ':%d' % line
    return shown


def format_ticks(ticks, hz):
    if hz is None:
        return '%d ticks' % ticks
    seconds = ticks / hz
    if seconds < 0.001:
        return '%.1f us' % (seconds * 1000000)
    if seconds < 1:
        return '%.3f ms' % (seconds * 1000)
    return '%.3f s' % seconds


def write_text(parts, x, y, text, size=13, weight=None, fill='#17202a',
               anchor=None, extra=''):
    attrs = [
        'x="%d"' % x,
        'y="%d"' % y,
        'font-size="%d"' % size,
        'fill="%s"' % fill,
    ]
    if weight:
        attrs.append('font-weight="%s"' % weight)
    if anchor:
        attrs.append('text-anchor="%s"' % anchor)
    if extra:
        attrs.append(extra)
    parts.append('<text %s>%s</text>' %
                 (' '.join(attrs), html.escape(text)))


def svg_report(frames, total_ticks, all_ticks, args):
    shown = frames[:args.limit]
    width = args.width
    margin = 28
    title_h = 96
    row_h = 26
    footer_h = 40
    height = title_h + row_h * (len(shown) + 1) + footer_h
    bar_x = 550
    value_x = width - margin
    bar_w = max(120, width - bar_x - 185)
    max_ticks = max((ticks for ticks, _, _, _ in shown), default=1)

    title = args.title
    if title is None:
        title = os.path.basename(args.profile if args.profile != '-' else 'profile')

    subtitle = [
        '%d grouped frame%s' % (len(frames), '' if len(frames) == 1 else 's'),
        '%d address%s' % (args.addresses, '' if args.addresses == 1 else 'es'),
        format_ticks(total_ticks, args.hz),
    ]
    if all_ticks != total_ticks:
        subtitle.append('%d ticks outside the selected program dropped'
                        % (all_ticks - total_ticks))
    if args.hz is None:
        subtitle.append('time values are raw ticks')
    else:
        subtitle.append('scaled at %.0f Hz' % args.hz)

    colors = {
        'bg': '#f7f8fa',
        'panel': '#ffffff',
        'line': '#d9dee7',
        'text': '#17202a',
        'muted': '#607080',
        'bar': '#2878bd',
        'bar2': '#6aaed6',
        'header': '#eef2f7',
    }

    parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
        'viewBox="0 0 %d %d">' % (width, height, width, height),
        '<style>'
        'text{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}'
        '.mono{font-family:"SFMono-Regular",Consolas,"Liberation Mono",monospace}'
        '</style>',
        '<rect width="100%%" height="100%%" fill="%s"/>' % colors['bg'],
        '<rect x="16" y="16" width="%d" height="%d" rx="6" fill="%s" '
        'stroke="%s"/>' % (width - 32, height - 32, colors['panel'],
                           colors['line']),
    ]

    write_text(parts, margin, 48, title, size=24, weight='700',
               fill=colors['text'])
    write_text(parts, margin, 74, ' - '.join(subtitle), size=13,
               fill=colors['muted'])

    header_y = title_h
    parts.append('<rect x="16" y="%d" width="%d" height="%d" fill="%s"/>'
                 % (header_y - 20, width - 32, row_h, colors['header']))
    write_text(parts, margin, header_y, 'Frame', size=12, weight='700',
               fill=colors['muted'])
    write_text(parts, 300, header_y, 'Source', size=12, weight='700',
               fill=colors['muted'])
    write_text(parts, bar_x, header_y, 'Self time', size=12, weight='700',
               fill=colors['muted'])
    write_text(parts, value_x, header_y, '%', size=12, weight='700',
               fill=colors['muted'], anchor='end')

    base_dir = os.path.dirname(os.path.abspath(args.elf)) if args.elf \
        else os.getcwd()
    for i, (ticks, name, path, line) in enumerate(shown):
        y = title_h + row_h * (i + 1)
        percent = ticks * 100.0 / total_ticks if total_ticks else 0
        w = max(1, int(round(bar_w * ticks / max_ticks)))
        source = source_name(path, line, base_dir)
        if i % 2:
            parts.append('<rect x="16" y="%d" width="%d" height="%d" '
                         'fill="#fbfcfe"/>' %
                         (y - 19, width - 32, row_h))
        write_text(parts, margin, y, shorten(name, 34), size=13,
                   fill=colors['text'])
        if name != shorten(name, 34):
            parts.append('<title>%s</title>' % html.escape(name))
        write_text(parts, 300, y, shorten(source, 32), size=12,
                   fill=colors['muted'], extra='class="mono"')
        parts.append('<rect x="%d" y="%d" width="%d" height="14" rx="2" '
                     'fill="%s"/>' % (bar_x, y - 12, w, colors['bar']))
        if w > 30:
            parts.append('<rect x="%d" y="%d" width="%d" height="14" rx="2" '
                         'fill="%s" opacity="0.18"/>' %
                         (bar_x, y - 12, max(1, w // 3), colors['bar2']))
        write_text(parts, bar_x + bar_w + 12, y,
                   format_ticks(ticks, args.hz), size=12,
                   fill=colors['text'], extra='class="mono"')
        write_text(parts, value_x, y, '%.2f' % percent, size=12,
                   fill=colors['text'], anchor='end', extra='class="mono"')

    if len(frames) > len(shown):
        rest = sum(ticks for ticks, _, _, _ in frames[len(shown):])
        y = title_h + row_h * (len(shown) + 1)
        write_text(parts, margin, y,
                   '%d more frame%s not shown, %s' %
                   (len(frames) - len(shown),
                    '' if len(frames) - len(shown) == 1 else 's',
                    format_ticks(rest, args.hz)),
                   size=12, fill=colors['muted'])

    parts.append('</svg>')
    return '\n'.join(parts) + '\n'


def main():
    parser = argparse.ArgumentParser(
        description='Turn "monitor prof save" output into a static SVG chart.')
    parser.add_argument('profile', metavar='FILE',
                        help='what "monitor prof save" wrote; - for stdin')
    parser.add_argument('-e', '--elf',
                        help='the ELF the program was linked as, for names')
    parser.add_argument('-b', '--base', type=parse_number, default=0,
                        help='load address to subtract first, octal unless '
                             '0x or a trailing dot says otherwise -- for a '
                             'PPU module, what ppu.gdb prints')
    parser.add_argument('-o', '--out',
                        help='where to write it (default: FILE.svg)')
    parser.add_argument('--by', choices=('line', 'function', 'address'),
                        default='line',
                        help='one bar per source line (default), per '
                             'function, or per address')
    parser.add_argument('--addr2line', default='pdp11-uknc-rt11-addr2line',
                        help='the addr2line to name addresses with')
    parser.add_argument('--hz', type=float,
                        help='ticks per second, to show real time instead '
                             'of raw ticks: 8000000 for the CPU, 6250000 '
                             'for the PPU')
    parser.add_argument('--limit', type=int, default=40,
                        help='how many bars to draw (default: 40)')
    parser.add_argument('--width', type=int, default=1200,
                        help='SVG width in pixels (default: 1200)')
    parser.add_argument('--title',
                        help='title to put at the top of the SVG')
    parser.add_argument('--only-program', action='store_true',
                        help='drop addresses that the ELF cannot name; useful '
                             'for a whole run that includes RT-11 boot time')
    args = parser.parse_args()

    if args.limit < 1:
        sys.exit('--limit must be at least 1')
    if args.width < 760:
        sys.exit('--width must be at least 760')
    if args.only_program and not args.elf:
        sys.exit('--only-program needs --elf')

    counts = read_profile(args.profile)
    if not counts:
        sys.exit('%s has no counted addresses in it' % args.profile)

    out = args.out
    if out is None:
        if args.profile == '-':
            sys.exit('reading standard input needs -o')
        out = os.path.splitext(args.profile)[0] + '.svg'

    where = symbolize([address for address, _ in counts],
                      args.elf, args.base, args.addr2line)
    all_ticks = sum(ticks for _, ticks in counts)
    if args.only_program:
        counts = [(address, ticks) for address, ticks in counts
                  if address in where
                  and (where[address][0] or where[address][1])]
        if not counts:
            sys.exit('no addresses from %s found in %s'
                     % (args.elf, args.profile))

    frames = group(counts, where, args.by)
    args.addresses = len(counts)
    total_ticks = sum(ticks for _, ticks in counts)
    with open(out, 'w') as f:
        f.write(svg_report(frames, total_ticks, all_ticks, args))

    named = sum(1 for _, _, path, _ in frames if path)
    print('written: %s, %d ticks over %d addresses in %d frames%s'
          % (out, total_ticks, len(counts), len(frames),
             '' if args.elf is None
             else ', %d of them with a source line' % named))


if __name__ == '__main__':
    main()
