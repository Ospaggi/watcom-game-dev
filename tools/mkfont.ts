#!/usr/bin/env bun
/**
 * mkfont.ts - Compile bitmap fonts to x86 glyph code for DOS VGA
 *
 * Reads 8x16 English font (4096B) and 16x16 Korean 8x4x4-bul font (11520B),
 * scans SRC/*.CPP for used characters, compiles to DIST/FONT.BIN.
 *
 * Usage:
 *   bun tools/mkfont.ts [--eng <path>] [--han <path>]
 */

import { existsSync, readFileSync, writeFileSync, readdirSync } from "fs";
import { parseArgs } from "util";

const { values } = parseArgs({
  args: Bun.argv.slice(2),
  options: {
    eng: { type: "string", default: "ASSETS/fonts/ENGLISH.FNT" },
    han: { type: "string", default: "ASSETS/fonts/HANGUL.FNT" },
  },
  allowPositionals: false,
});

// ---- Load font files ----

const ASCII_5X7: Record<string, number[]> = {
  "0": [0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e],
  "1": [0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e],
  "2": [0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f],
  "3": [0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e],
  "4": [0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02],
  "5": [0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e],
  "6": [0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e],
  "7": [0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08],
  "8": [0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e],
  "9": [0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e],
  "A": [0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11],
  "B": [0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e],
  "C": [0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e],
  "D": [0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e],
  "E": [0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f],
  "F": [0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10],
  "G": [0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f],
  "H": [0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11],
  "I": [0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e],
  "J": [0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c],
  "K": [0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11],
  "L": [0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f],
  "M": [0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11],
  "N": [0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11],
  "O": [0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e],
  "P": [0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10],
  "Q": [0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d],
  "R": [0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11],
  "S": [0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e],
  "T": [0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04],
  "U": [0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e],
  "V": [0x11, 0x11, 0x11, 0x11, 0x0a, 0x0a, 0x04],
  "W": [0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a],
  "X": [0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11],
  "Y": [0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04],
  "Z": [0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f],
  "-": [0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00],
  ".": [0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c],
  ":": [0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x00],
};

function makeFallbackEngFont(): Buffer {
  const font = Buffer.alloc(4096);
  for (const [ch, rows] of Object.entries(ASCII_5X7)) {
    const cp = ch.charCodeAt(0);
    for (let y = 0; y < 7; y++) {
      let out = 0;
      for (let x = 0; x < 5; x++) {
        if (rows[y] & (0x10 >> x)) out |= 0x80 >> (x + 1);
      }
      font[cp * 16 + 2 + y * 2] = out;
      font[cp * 16 + 3 + y * 2] = out;
    }
  }
  return font;
}

function resolveHanPath(path: string): string {
  if (existsSync(path)) return path;
  const candidates = [
    "/Users/gcjjyy/dos/MAX/HANGUL.FNT",
    "/Users/gcjjyy/dos/GAMES/KHAN2/WONJO/HANGUL.FNT",
    "/Users/gcjjyy/dos/GAMES/ASTONISH/NORMAL.FNT",
    "/Users/gcjjyy/dos/GAMES/DARKSIDE/HANGUL.FNT",
    "/Users/gcjjyy/dos/TB20/TBG1.FNT",
    "/Users/gcjjyy/lab/oscc/imsplay/public/HANBG.FNT",
    "/Users/gcjjyy/lab/imsplay/public/HANBG.FNT",
  ];
  for (const c of candidates) {
    if (existsSync(c)) return c;
  }
  return path;
}

function resolveEngPath(path: string): string {
  if (existsSync(path)) return path;
  const candidates = [
    "/Users/gcjjyy/dos/MAX/ENGLISH.FNT",
    "/Users/gcjjyy/dos/GAMES/KHAN2/WONJO/ENGLISH.FNT",
    "/Users/gcjjyy/dos/GAMES/SAMHERO/ENG.FNT",
    "/Users/gcjjyy/dos/GAT10/GAT.FNT",
  ];
  for (const c of candidates) {
    if (existsSync(c)) return c;
  }
  return path;
}

let engFont: Buffer;
const engPath = resolveEngPath(values.eng!);
if (existsSync(engPath)) {
  engFont = readFileSync(engPath);
  if (engFont.length !== 4096)
    throw new Error(`English font must be 4096 bytes, got ${engFont.length}`);
} else {
  engFont = makeFallbackEngFont();
}

const hanPath = resolveHanPath(values.han!);
const hanRaw = readFileSync(hanPath);
const hanFont = Buffer.alloc(11520);
if (hanRaw.length < 11008)
  throw new Error(`Korean font is too small, got ${hanRaw.length}`);
hanRaw.copy(hanFont, 0, 0, Math.min(hanRaw.length, hanFont.length));

// ---- Scan SRC/*.CPP for characters used in string literals ----

function scanSourceChars(): Set<number> {
  const chars = new Set<number>();
  const files = readdirSync("SRC").filter(
    (f) => f.endsWith(".CPP") || f.endsWith(".H"),
  );

  for (const file of files) {
    const src = readFileSync(`SRC/${file}`, "utf-8");
    const re = /"((?:[^"\\]|\\.)*)"/g;
    let m: RegExpExecArray | null;
    while ((m = re.exec(src)) !== null) {
      const raw = m[1];
      let i = 0;
      while (i < raw.length) {
        if (raw[i] === "\\") { i += 2; continue; }
        const cp = raw.codePointAt(i)!;
        if (cp >= 0x20 && cp <= 0x7e) chars.add(cp);
        else if (cp >= 0xac00 && cp <= 0xd7a3) chars.add(cp);
        i += cp > 0xffff ? 2 : 1;
      }
    }
  }

  // Always include ASCII printable range
  for (let c = 0x20; c <= 0x7e; c++) chars.add(c);
  return chars;
}

const usedChars = scanSourceChars();
console.error(
  `Found ${usedChars.size} unique characters (${[...usedChars].filter((c) => c >= 0xac00).length} Korean)`,
);

// ---- Korean decomposition and bul selection ----

function decomposeHangul(cp: number): {
  cho: number;
  jung: number;
  jong: number;
} {
  const idx = cp - 0xac00;
  return {
    cho: Math.floor(idx / 588),
    jung: Math.floor((idx % 588) / 28),
    jong: idx % 28,
  };
}

const CHO_BUL: number[][] = [
  [0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 3, 3, 1, 2, 4, 4, 4, 2, 1, 3, 0],
  [5, 5, 5, 5, 5, 5, 5, 5, 6, 7, 7, 7, 6, 6, 7, 7, 7, 6, 6, 7, 5],
];
const JONG_BUL = [
  0, 2, 0, 2, 1, 2, 1, 2, 3, 0, 2, 1, 3, 3, 1, 2, 1, 3, 3, 1, 1,
];

function getChoBul(jung: number, hasJong: boolean): number {
  return CHO_BUL[hasJong ? 1 : 0][jung];
}
function getJungBul(cho: number, hasJong: boolean): number {
  return (cho === 0 || cho === 16 ? 0 : 1) + (hasJong ? 2 : 0);
}
function getJongBul(jung: number): number {
  return JONG_BUL[jung];
}

// ---- Korean glyph composition ----

function composeHangulBitmap(cp: number): Uint8Array {
  const { cho, jung, jong } = decomposeHangul(cp);
  const hasJong = jong > 0;
  const choBul = getChoBul(jung, hasJong);
  const jungBul = getJungBul(cho, hasJong);
  const jongBul = hasJong ? getJongBul(jung) : 0;

  /* MAX/HANGUL.FNT is a padded 8-bul johab bitmap (360 glyphs, each set has a
   * blank slot 0):
   *   초성 8 bul x 20  -> indices   0..159  (cho+1, slot 0 blank)
   *   중성 4 bul x 22  -> indices 160..247  (jung+1, slot 0 blank)
   *   종성 4 bul x 28  -> indices 248..359  (jong 0..27, slot 0 = no jongseong) */
  const choIdx = choBul * 20 + (cho + 1);
  const jungIdx = 160 + jungBul * 22 + (jung + 1);
  const jongIdx = 248 + jongBul * 28 + jong;

  const result = new Uint8Array(32);
  for (let i = 0; i < 32; i++) {
    result[i] =
      hanFont[choIdx * 32 + i] |
      hanFont[jungIdx * 32 + i] |
      hanFont[jongIdx * 32 + i];
  }
  return result;
}

// ---- x86 compiled glyph code gen ----
// MOV [EDI+disp32], EAX  -> 89 87 xx xx xx xx       (4 pixels)
// MOV [EDI+disp32], AX   -> 66 89 87 xx xx xx xx    (2 pixels)
// MOV [EDI+disp32], AL   -> 88 87 xx xx xx xx       (1 pixel)
// RET                    -> C3

function compileFontGlyph(
  w: number,
  h: number,
  bitmap1bit: Uint8Array,
): Uint8Array {
  const code: number[] = [];
  const stride = 320;

  const pushLE32 = (v: number) => {
    code.push(v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff);
  };

  const bytesPerRow = Math.ceil(w / 8);

  for (let y = 0; y < h; y++) {
    let x = 0;
    while (x < w) {
      const byteIdx = y * bytesPerRow + Math.floor(x / 8);
      const bitIdx = 7 - (x % 8);
      if (!((bitmap1bit[byteIdx] >> bitIdx) & 1)) {
        x++;
        continue;
      }

      const runStart = x;
      while (x < w) {
        const bi = y * bytesPerRow + Math.floor(x / 8);
        const bt = 7 - (x % 8);
        if (!((bitmap1bit[bi] >> bt) & 1)) break;
        x++;
      }
      const runLen = x - runStart;

      const baseOff = y * stride + runStart;
      let pos = 0;
      while (pos < runLen) {
        const remaining = runLen - pos;
        const off = baseOff + pos;

        if (remaining >= 4) {
          code.push(0x89, 0x87);
          pushLE32(off);
          pos += 4;
        } else if (remaining >= 2) {
          code.push(0x66, 0x89, 0x87);
          pushLE32(off);
          pos += 2;
        } else {
          code.push(0x88, 0x87);
          pushLE32(off);
          pos += 1;
        }
      }
    }
  }

  code.push(0xc3); // RET
  return new Uint8Array(code);
}

// ---- Build all glyphs ----

interface GlyphEntry {
  codepoint: number;
  code: Uint8Array;
}
const glyphs: GlyphEntry[] = [];

for (const cp of [...usedChars].sort((a, b) => a - b)) {
  let code: Uint8Array;
  if (cp >= 0x20 && cp <= 0x7e) {
    const bitmap = engFont.subarray(cp * 16, cp * 16 + 16);
    code = compileFontGlyph(8, 16, bitmap);
  } else if (cp >= 0xac00 && cp <= 0xd7a3) {
    const bitmap = composeHangulBitmap(cp);
    code = compileFontGlyph(16, 16, bitmap);
  } else {
    continue;
  }
  glyphs.push({ codepoint: cp, code });
}

// ---- Write FONT.BIN ----

const headerSize = 8;
const indexEntrySize = 10;
const indexSize = glyphs.length * indexEntrySize;
let totalCode = 0;
for (const g of glyphs) totalCode += g.code.length;

const bin = Buffer.alloc(headerSize + indexSize + totalCode);
let boff = 0;

bin.write("CFNT", 0, 4, "ascii");
boff += 4;
bin.writeUInt16LE(glyphs.length, boff);
boff += 2;
bin[boff++] = 8;
bin[boff++] = 16;

let codeOff = 0;
for (const g of glyphs) {
  bin.writeUInt32LE(g.codepoint, boff);
  boff += 4;
  bin.writeUInt32LE(codeOff, boff);
  boff += 4;
  bin.writeUInt16LE(g.code.length, boff);
  boff += 2;
  codeOff += g.code.length;
}

for (const g of glyphs) {
  Buffer.from(g.code).copy(bin, boff);
  boff += g.code.length;
}

writeFileSync("DIST/FONT.BIN", bin);
console.error(
  `Compiled ${glyphs.length} glyphs from ${engPath} + ${hanPath} -> DIST/FONT.BIN (${bin.length} bytes)`,
);
