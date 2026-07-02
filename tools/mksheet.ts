#!/usr/bin/env bun
/**
 * mksheet.ts - Compose 16x16 cells from source PNGs into a single-row sheet PNG.
 *
 * Usage:
 *   bun tools/mksheet.ts <cells.json> <OUT.PNG>
 *     cells.json: [{ "src": "path.png", "cx": 0, "cy": 1 }, ...]
 *     (cx, cy are 16px cell coordinates; order = frame order)
 *
 *   bun tools/mksheet.ts --cell <src.png> <cx> <cy> <out.png> [zoom]
 *     Extract a single cell (optionally zoomed) for visual inspection.
 */

import { readFileSync, writeFileSync } from "fs";

const CELL = 16;

// ---- PNG decode (same logic as tools/mksprite.ts) ----

function paethPredictor(a: number, b: number, c: number): number {
  const p = a + b - c;
  const pa = Math.abs(p - a);
  const pb = Math.abs(p - b);
  const pc = Math.abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

function unfilterRow(
  filter: number, row: Uint8Array,
  prior: Uint8Array | null, bpp: number,
): void {
  switch (filter) {
    case 0: break;
    case 1:
      for (let i = bpp; i < row.length; i++)
        row[i] = (row[i] + row[i - bpp]) & 0xFF;
      break;
    case 2:
      if (prior)
        for (let i = 0; i < row.length; i++)
          row[i] = (row[i] + prior[i]) & 0xFF;
      break;
    case 3:
      for (let i = 0; i < row.length; i++) {
        const a = i >= bpp ? row[i - bpp] : 0;
        const b = prior ? prior[i] : 0;
        row[i] = (row[i] + ((a + b) >> 1)) & 0xFF;
      }
      break;
    case 4:
      for (let i = 0; i < row.length; i++) {
        const a = i >= bpp ? row[i - bpp] : 0;
        const b = prior ? prior[i] : 0;
        const c = i >= bpp && prior ? prior[i - bpp] : 0;
        row[i] = (row[i] + paethPredictor(a, b, c)) & 0xFF;
      }
      break;
  }
}

function readPNG(path: string): { width: number; height: number; rgba: Uint8Array } {
  const buf = readFileSync(path);
  if (buf[0] !== 0x89 || buf[1] !== 0x50) throw new Error(`Not a PNG: ${path}`);

  let offset = 8;
  let width = 0, height = 0, bitDepth = 0, colorType = 0;
  const idatChunks: Buffer[] = [];
  let plte: Uint8Array | null = null;
  let trns: Uint8Array | null = null;

  while (offset < buf.length) {
    const length = buf.readUInt32BE(offset);
    const type = buf.toString("ascii", offset + 4, offset + 8);
    const data = buf.subarray(offset + 8, offset + 8 + length);
    if (type === "IHDR") {
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      bitDepth = data[8];
      colorType = data[9];
    } else if (type === "PLTE") plte = new Uint8Array(data);
    else if (type === "tRNS") trns = new Uint8Array(data);
    else if (type === "IDAT") idatChunks.push(Buffer.from(data));
    else if (type === "IEND") break;
    offset += 12 + length;
  }

  if (bitDepth !== 8) throw new Error(`Only 8-bit PNG supported: ${path}`);
  const bppMap: Record<number, number> = { 2: 3, 3: 1, 6: 4 };
  const bpp = bppMap[colorType];
  if (!bpp) throw new Error(`Unsupported color type ${colorType}: ${path}`);

  const compressed = Buffer.concat(idatChunks);
  const raw = Buffer.from(Bun.inflateSync(compressed.subarray(2)));
  const rowBytes = width * bpp;
  const pixels = new Uint8Array(height * rowBytes);
  let srcPos = 0;
  for (let y = 0; y < height; y++) {
    const filter = raw[srcPos++];
    const row = pixels.subarray(y * rowBytes, (y + 1) * rowBytes);
    const prior = y > 0 ? pixels.subarray((y - 1) * rowBytes, y * rowBytes) : null;
    for (let i = 0; i < rowBytes; i++) row[i] = raw[srcPos++];
    unfilterRow(filter, row, prior, bpp);
  }

  const rgba = new Uint8Array(width * height * 4);
  if (colorType === 6) rgba.set(pixels);
  else if (colorType === 2) {
    for (let i = 0; i < width * height; i++) {
      rgba[i * 4] = pixels[i * 3];
      rgba[i * 4 + 1] = pixels[i * 3 + 1];
      rgba[i * 4 + 2] = pixels[i * 3 + 2];
      rgba[i * 4 + 3] = 255;
    }
  } else {
    if (!plte) throw new Error(`Indexed PNG missing PLTE: ${path}`);
    for (let i = 0; i < width * height; i++) {
      const idx = pixels[i];
      rgba[i * 4] = plte[idx * 3];
      rgba[i * 4 + 1] = plte[idx * 3 + 1];
      rgba[i * 4 + 2] = plte[idx * 3 + 2];
      rgba[i * 4 + 3] = trns && idx < trns.length ? trns[idx] : 255;
    }
  }
  return { width, height, rgba };
}

// ---- PNG encode (RGBA, no filter) ----

function crc32(data: Uint8Array): number {
  let crc = 0xFFFFFFFF;
  for (let i = 0; i < data.length; i++) {
    crc ^= data[i];
    for (let k = 0; k < 8; k++)
      crc = (crc >>> 1) ^ (0xEDB88320 & -(crc & 1));
  }
  return (crc ^ 0xFFFFFFFF) >>> 0;
}

function adler32(data: Uint8Array): number {
  let a = 1, b = 0;
  for (let i = 0; i < data.length; i++) {
    a = (a + data[i]) % 65521;
    b = (b + a) % 65521;
  }
  return ((b << 16) | a) >>> 0;
}

function chunk(type: string, data: Uint8Array): Buffer {
  const out = Buffer.alloc(12 + data.length);
  out.writeUInt32BE(data.length, 0);
  out.write(type, 4, "ascii");
  Buffer.from(data).copy(out, 8);
  const crcBuf = Buffer.alloc(4 + data.length);
  crcBuf.write(type, 0, "ascii");
  Buffer.from(data).copy(crcBuf, 4);
  out.writeUInt32BE(crc32(crcBuf), 8 + data.length);
  return out;
}

function writePNG(path: string, width: number, height: number, rgba: Uint8Array): void {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0);
  ihdr.writeUInt32BE(height, 4);
  ihdr[8] = 8;   // bit depth
  ihdr[9] = 6;   // RGBA
  const scan = Buffer.alloc(height * (1 + width * 4));
  for (let y = 0; y < height; y++) {
    scan[y * (1 + width * 4)] = 0; // filter none
    Buffer.from(rgba.subarray(y * width * 4, (y + 1) * width * 4))
      .copy(scan, y * (1 + width * 4) + 1);
  }
  const deflated = Buffer.from(Bun.deflateSync(scan));
  const idat = Buffer.alloc(2 + deflated.length + 4);
  idat[0] = 0x78; idat[1] = 0x01;           // zlib header
  deflated.copy(idat, 2);
  idat.writeUInt32BE(adler32(scan), 2 + deflated.length);
  const png = Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]),
    chunk("IHDR", ihdr),
    chunk("IDAT", idat),
    chunk("IEND", new Uint8Array(0)),
  ]);
  writeFileSync(path, png);
}

// ---- cell ops ----

function copyCell(
  src: { width: number; height: number; rgba: Uint8Array },
  cx: number, cy: number,
  dst: Uint8Array, dstW: number, dx: number, dy: number,
): void {
  for (let y = 0; y < CELL; y++) {
    for (let x = 0; x < CELL; x++) {
      const si = ((cy * CELL + y) * src.width + cx * CELL + x) * 4;
      const di = ((dy + y) * dstW + dx + x) * 4;
      dst[di] = src.rgba[si];
      dst[di + 1] = src.rgba[si + 1];
      dst[di + 2] = src.rgba[si + 2];
      dst[di + 3] = src.rgba[si + 3];
    }
  }
}

// ---- main ----

const args = process.argv.slice(2);

if (args[0] === "--cell") {
  const [, src, cxs, cys, out, zooms] = args;
  const cx = parseInt(cxs), cy = parseInt(cys);
  const zoom = zooms ? parseInt(zooms) : 1;
  const img = readPNG(src);
  const cell = new Uint8Array(CELL * CELL * 4);
  copyCell(img, cx, cy, cell, CELL, 0, 0);
  const z = new Uint8Array(CELL * zoom * CELL * zoom * 4);
  for (let y = 0; y < CELL * zoom; y++)
    for (let x = 0; x < CELL * zoom; x++)
      for (let c = 0; c < 4; c++)
        z[(y * CELL * zoom + x) * 4 + c] =
          cell[((y / zoom | 0) * CELL + (x / zoom | 0)) * 4 + c];
  writePNG(out, CELL * zoom, CELL * zoom, z);
  console.error(`cell (${cx},${cy}) of ${src} -> ${out} (${zoom}x)`);
} else {
  const [jsonPath, outPath] = args;
  if (!jsonPath || !outPath) {
    console.error("Usage: bun tools/mksheet.ts <cells.json> <OUT.PNG>");
    console.error("       bun tools/mksheet.ts --cell <src.png> <cx> <cy> <out.png> [zoom]");
    process.exit(1);
  }
  const cells: { src: string; cx: number; cy: number }[] =
    JSON.parse(readFileSync(jsonPath, "utf-8"));
  const sheet = new Uint8Array(cells.length * CELL * CELL * 4);
  const cache: Record<string, ReturnType<typeof readPNG>> = {};
  cells.forEach((c, i) => {
    if (!cache[c.src]) cache[c.src] = readPNG(c.src);
    copyCell(cache[c.src], c.cx, c.cy, sheet, cells.length * CELL, i * CELL, 0);
  });
  writePNG(outPath, cells.length * CELL, CELL, sheet);
  console.error(`${cells.length} cells -> ${outPath}`);
}
