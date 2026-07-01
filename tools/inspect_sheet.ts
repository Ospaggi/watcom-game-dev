#!/usr/bin/env bun
/**
 * inspect_sheet.ts - Visualize alpha channel of a PNG to spot frame boundaries.
 *
 * Prints a per-column "non-transparent pixel count" bar, plus an ASCII
 * mini-map of the image (one char per pixel, scaled).
 *
 * Usage: bun tools/inspect_sheet.ts <image.png>
 */
import { readFileSync } from "fs";
import { inflateSync } from "zlib";

function readPNG(path: string): { w: number; h: number; rgba: Uint8Array } {
  const buf = readFileSync(path);
  const w = (buf[16]<<24)|(buf[17]<<16)|(buf[18]<<8)|buf[19];
  const h = (buf[20]<<24)|(buf[21]<<16)|(buf[22]<<8)|buf[23];
  const colorType = buf[25];
  if (buf[24] !== 8 || colorType !== 6) {
    console.error(`only 8-bit RGBA supported (got bit=${buf[24]} type=${colorType})`);
    process.exit(1);
  }

  const idatChunks: Buffer[] = [];
  let off = 8;
  while (off < buf.length) {
    const len = (buf[off]<<24)|(buf[off+1]<<16)|(buf[off+2]<<8)|buf[off+3];
    const type = String.fromCharCode(buf[off+4], buf[off+5], buf[off+6], buf[off+7]);
    if (type === "IDAT") idatChunks.push(buf.slice(off+8, off+8+len));
    if (type === "IEND") break;
    off += 12 + len;
  }
  const data = inflateSync(Buffer.concat(idatChunks));

  const rgba = new Uint8Array(w * h * 4);
  let di = 0, si = 0;
  let prior = new Uint8Array(w * 4);
  for (let y = 0; y < h; y++) {
    const filter = data[si++];
    const row = data.slice(si, si + w * 4);
    si += w * 4;
    const out = new Uint8Array(row);
    for (let x = 0; x < w * 4; x++) {
      const a = x >= 4 ? out[x-4] : 0;
      const b = prior[x];
      const c = x >= 4 ? prior[x-4] : 0;
      switch (filter) {
        case 0: break;
        case 1: out[x] = (out[x] + a) & 0xFF; break;
        case 2: out[x] = (out[x] + b) & 0xFF; break;
        case 3: out[x] = (out[x] + ((a + b) >> 1)) & 0xFF; break;
        case 4: {
          const p = a + b - c;
          const pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
          const pred = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
          out[x] = (out[x] + pred) & 0xFF;
          break;
        }
      }
    }
    rgba.set(out, di);
    di += w * 4;
    prior = out;
  }
  return { w, h, rgba };
}

const path = Bun.argv[2];
if (!path) { console.error("usage: bun inspect_sheet.ts <image.png>"); process.exit(1); }

const { w, h, rgba } = readPNG(path);
console.log(`${path}: ${w}x${h}`);

console.log("\nColumn alpha density (chars = non-transparent pixel count, 0=empty):");
let header = "  ";
for (let x = 0; x < w; x++) header += (x % 10 === 0) ? Math.floor(x/10) : " ";
console.log(header);
let sub = "  ";
for (let x = 0; x < w; x++) sub += (x % 10).toString();
console.log(sub);

let bar = "  ";
for (let x = 0; x < w; x++) {
  let count = 0;
  for (let y = 0; y < h; y++) if (rgba[(y*w+x)*4 + 3] >= 128) count++;
  bar += count === 0 ? "." : count < 5 ? "·" : count < 12 ? ":" : count < 20 ? "+" : "#";
}
console.log(bar);

console.log("\nMini-map (one char per pixel, '.' transparent, '#' opaque):");
for (let y = 0; y < h; y++) {
  let row = (y % 10 === 0 ? y.toString().padStart(3) + " " : "    ");
  for (let x = 0; x < w; x++) {
    row += rgba[(y*w+x)*4 + 3] >= 128 ? "#" : ".";
  }
  console.log(row);
}
