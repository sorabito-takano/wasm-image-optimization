import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFile, readdir } from "node:fs/promises";
import { basename, join, resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { brotliCompressSync, constants, crc32, deflateSync, gzipSync } from "node:zlib";

const directories = process.argv.slice(2);
assert.ok(directories.length > 0, "Pass baseline and candidate ESM directories");
const imageDirectory = new URL("../images/", import.meta.url);
const files = (await readdir(imageDirectory))
  .filter((name) => /\.(jpe?g|png|webp)$/i.test(name))
  .sort();
assert.ok(files.length > 0, "No input images found");
const inputs = await Promise.all(
  files.map(async (name) => ({
    name,
    data: new Uint8Array(await readFile(new URL(name, imageDirectory))),
  })),
);
const pngChunk = (type, data) => {
  const payload = Buffer.concat([Buffer.from(type), data]);
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length);
  const checksum = Buffer.alloc(4);
  checksum.writeUInt32BE(crc32(payload));
  return Buffer.concat([length, payload, checksum]);
};
for (const width of [1, 4, 5]) {
  const header = Buffer.alloc(13);
  header.writeUInt32BE(width, 0);
  header.writeUInt32BE(1, 4);
  header[8] = 8;
  header[9] = 2;
  const pixels = Buffer.alloc(1 + width * 3, 127);
  pixels[0] = 0;
  inputs.push({
    name: `generated-${width}x1.png`,
    data: new Uint8Array(Buffer.concat([
      Buffer.from("89504e470d0a1a0a", "hex"),
      pngChunk("IHDR", header),
      pngChunk("IDAT", deflateSync(pixels)),
      pngChunk("IEND", Buffer.alloc(0)),
    ])),
  });
}
const scenarios = [
  { format: "webp", width: 320, height: 0 },
  { format: "jpeg", width: 320, height: 0 },
  { format: "webp", width: 0, height: 240 },
  { format: "none", width: 0, height: 0 },
];
const baseline = new Map();
const hash = (data) => createHash("sha256").update(data).digest("hex");
const median = (values) => [...values].sort((left, right) => left - right)[Math.floor(values.length / 2)];
const reports = [];

for (const [variantIndex, directory] of directories.entries()) {
  const absoluteDirectory = resolve(directory);
  const wasm = await readFile(join(absoluteDirectory, "libImage.wasm"));
  const glue = await readFile(join(absoluteDirectory, "libImage.js"));
  const compiled = await WebAssembly.compile(wasm);
  let memory;
  const { default: createModule } = await import(
    pathToFileURL(join(absoluteDirectory, "libImage.js")).href
  );
  const module = await createModule({
    wasmBinary: wasm,
    print: () => {},
    instantiateWasm(imports, receiveInstance) {
      const instance = new WebAssembly.Instance(compiled, imports);
      memory = Object.values(instance.exports).find(
        (value) => value instanceof WebAssembly.Memory,
      );
      receiveInstance(instance, compiled);
      return instance.exports;
    },
  });
  assert.ok(memory, "WASM must export its memory");
  const initialMemory = memory.buffer.byteLength;
  const originalLog = console.log;
  console.log = () => {};
  try {
    for (let length = 0; length < 16; length++) {
      assert.equal(module.optimize(new Uint8Array(length), 0, 0, 80, "webp"), null);
      module.releaseResult();
    }
  } finally {
    console.log = originalLog;
  }
  const cases = [];
  const memoryAfterRounds = [];

  const run = (input, scenario) => {
    const originalLog = console.log;
    console.log = () => {};
    try {
      const result = module.optimize(
        input.data, scenario.width, scenario.height, 80, scenario.format,
      );
      assert.ok(result?.data?.length, `${input.name}: empty conversion result`);
      const data = Uint8Array.from(result.data);
      const { originalWidth, originalHeight, width, height } = result;
      for (const dimension of [originalWidth, originalHeight, width, height]) {
        assert.ok(Number.isFinite(dimension) && dimension > 0);
      }
      if (input.name.startsWith("generated-")) {
        const expectedWidth = Number(input.name.match(/generated-(\d+)/)[1]);
        assert.equal(originalWidth, expectedWidth);
        assert.equal(originalHeight, 1);
        assert.equal(width, expectedWidth);
        assert.equal(height, 1);
      }
      if (scenario.format === "none") assert.deepEqual(data, input.data);
      if (scenario.format === "jpeg") assert.equal(Buffer.from(data.subarray(0, 2)).toString("hex"), "ffd8");
      if (scenario.format === "webp") assert.equal(Buffer.from(data.subarray(8, 12)).toString(), "WEBP");
      return { hash: hash(data), bytes: data.length, originalWidth, originalHeight, width, height };
    } finally {
      console.log = originalLog;
      module.releaseResult();
    }
  };

  for (const input of inputs) {
    for (const scenario of scenarios) {
      const key = `${input.name}:${scenario.format}:${scenario.width}x${scenario.height}`;
      const result = run(input, scenario);
      if (variantIndex === 0) baseline.set(key, result);
      else assert.deepEqual(result, baseline.get(key), `${directory}: ${key}`);
      cases.push({ key, input, scenario, milliseconds: [] });
    }
  }
  const warmedMemory = memory.buffer.byteLength;
  for (let round = 0; round < 3; round++) {
    for (const entry of cases) {
      const started = performance.now();
      const result = run(entry.input, entry.scenario);
      entry.milliseconds.push(performance.now() - started);
      assert.deepEqual(result, baseline.get(entry.key), `${directory}: repeat ${entry.key}`);
    }
    memoryAfterRounds.push(memory.buffer.byteLength);
  }
  const brotliOptions = { params: { [constants.BROTLI_PARAM_QUALITY]: 11 } };
  reports.push({
    variant: basename(resolve(absoluteDirectory, "..")),
    directory: absoluteDirectory,
    wasmBytes: wasm.length,
    wasmGzipBytes: gzipSync(wasm, { level: 9 }).length,
    wasmBrotliBytes: brotliCompressSync(wasm, brotliOptions).length,
    jsBytes: glue.length,
    initialMemory,
    shortInputsChecked: 16,
    warmedMemory,
    memoryAfterRounds,
    cases: cases.map(({ key, milliseconds }) => ({
      key,
      medianMs: Number(median(milliseconds).toFixed(3)),
      ...baseline.get(key),
    })),
    summedMedianMs: Number(cases.reduce((total, entry) => total + median(entry.milliseconds), 0).toFixed(3)),
  });
}

console.log(JSON.stringify({ node: process.version, rounds: 3, reports }, null, 2));