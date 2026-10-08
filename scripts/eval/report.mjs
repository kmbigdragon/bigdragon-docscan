#!/usr/bin/env node
// Summarises docscan-bench results with statistics that can back a claim such as "B is better
// than A". See docs/EVALUATION.md for the definitions.
//
//   node scripts/eval/report.mjs <results.csv> [--baseline <results.csv>] [--by category|split]
//                                [--worst <n>] [--json <summary.json>]
//
// Confidence intervals are 95 % cluster-bootstrap intervals: whole groups (videos) are resampled,
// because frames of one video are strongly correlated and are not independent samples.

import { readFileSync, writeFileSync } from 'node:fs';
import { parseArgs } from 'node:util';

const { values: options, positionals } = parseArgs({
  allowPositionals: true,
  options: {
    baseline: { type: 'string' },
    by: { type: 'string', default: 'category' },
    json: { type: 'string' },
    resamples: { type: 'string', default: '2000' },
    worst: { type: 'string', default: '0' },
  },
});
if (positionals.length !== 1) {
  console.error('Usage: node scripts/eval/report.mjs <results.csv> [--baseline <results.csv>] [--by category|split] [--json out.json]');
  process.exit(1);
}
const RESAMPLES = Number(options.resamples);
const SUCCESS = 0.95; // a frame counts as "correct" from this Jaccard index on

// ---- Input ---------------------------------------------------------------------------------------

function readResults(file) {
  const [header, ...lines] = readFileSync(file, 'utf8').trim().split(/\r?\n/);
  const columns = header.split(',');
  const numeric = new Set(['loaded', 'found', 'confidence', 'jaccard', 'corner_err_mean', 'corner_err_max', 'corner_err_px', 'detect_ms']);
  return lines.map((line) => {
    const fields = line.split(',');
    const row = {};
    columns.forEach((name, i) => (row[name] = numeric.has(name) ? Number(fields[i]) : fields[i]));
    return row;
  });
}

// ---- Statistics ----------------------------------------------------------------------------------

/** Small deterministic PRNG so reports are reproducible. */
function mulberry32(seed) {
  return () => {
    seed = (seed + 0x6d2b79f5) | 0;
    let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function quantile(sorted, q) {
  if (sorted.length === 0) return NaN;
  const position = (sorted.length - 1) * q;
  const low = Math.floor(position);
  const high = Math.ceil(position);
  return sorted[low] + (sorted[high] - sorted[low]) * (position - low);
}

/** Mean of value(row) with a 95 % cluster-bootstrap CI (clusters = row.group). */
function meanWithCi(rows, value) {
  const clusters = new Map();
  for (const row of rows) {
    const cluster = clusters.get(row.group) ?? { sum: 0, n: 0 };
    cluster.sum += value(row);
    cluster.n += 1;
    clusters.set(row.group, cluster);
  }
  const list = [...clusters.values()];
  const total = list.reduce((acc, c) => ({ sum: acc.sum + c.sum, n: acc.n + c.n }), { sum: 0, n: 0 });
  const random = mulberry32(20151023);
  const means = new Float64Array(RESAMPLES);
  for (let b = 0; b < RESAMPLES; b++) {
    let sum = 0;
    let n = 0;
    for (let i = 0; i < list.length; i++) {
      const cluster = list[Math.floor(random() * list.length)];
      sum += cluster.sum;
      n += cluster.n;
    }
    means[b] = sum / n;
  }
  means.sort();
  return { mean: total.sum / total.n, low: quantile(means, 0.025), high: quantile(means, 0.975) };
}

/** Area under the ROC curve of `score` for separating positives from negatives (ties = 0.5). */
function auroc(rows, score, isPositive) {
  const sorted = rows.map((r) => ({ s: score(r), p: isPositive(r) })).sort((a, b) => a.s - b.s);
  let rankSum = 0;
  let positives = 0;
  for (let i = 0; i < sorted.length; ) {
    let j = i;
    while (j < sorted.length && sorted[j].s === sorted[i].s) j++;
    const averageRank = (i + 1 + j) / 2;
    for (let k = i; k < j; k++) {
      if (sorted[k].p) {
        rankSum += averageRank;
        positives++;
      }
    }
    i = j;
  }
  const negatives = sorted.length - positives;
  if (positives === 0 || negatives === 0) return NaN;
  return (rankSum - (positives * (positives + 1)) / 2) / (positives * negatives);
}

function summarize(rows) {
  const jaccard = rows.map((r) => r.jaccard).sort((a, b) => a - b);
  const time = rows.map((r) => r.detect_ms).sort((a, b) => a - b);
  const cornerMax = rows.map((r) => r.corner_err_max).sort((a, b) => a - b);
  return {
    frames: rows.length,
    groups: new Set(rows.map((r) => r.group)).size,
    jaccard: meanWithCi(rows, (r) => r.jaccard),
    jaccardMedian: quantile(jaccard, 0.5),
    jaccardP10: quantile(jaccard, 0.1),
    success90: meanWithCi(rows, (r) => (r.jaccard >= 0.9 ? 1 : 0)),
    success95: meanWithCi(rows, (r) => (r.jaccard >= SUCCESS ? 1 : 0)),
    foundRate: rows.filter((r) => r.found).length / rows.length,
    cornerErrorMean: meanWithCi(rows, (r) => r.corner_err_mean),
    cornerErrorMaxP95: quantile(cornerMax, 0.95),
    confidenceAuroc: auroc(rows, (r) => r.confidence, (r) => r.jaccard >= SUCCESS),
    timeP50: quantile(time, 0.5),
    timeP95: quantile(time, 0.95),
    unreadable: rows.filter((r) => !r.loaded).length,
  };
}

// ---- Output --------------------------------------------------------------------------------------

const f3 = (x) => (Number.isFinite(x) ? x.toFixed(4) : 'n/a');
const pct = (x) => (Number.isFinite(x) ? `${(100 * x).toFixed(2)} %` : 'n/a');
const ci = (m, format = f3) => `${format(m.mean)} [${format(m.low)}, ${format(m.high)}]`;

const rows = readResults(positionals[0]);
const summary = summarize(rows);
const output = [];
output.push(`# docscan-bench: ${positionals[0]}`, '');
output.push(`${summary.frames} frames in ${summary.groups} groups. Brackets: 95 % cluster-bootstrap CI.`, '');
output.push('| Metric | Value |', '| --- | --- |');
output.push(`| Mean Jaccard index (SmartDoc protocol) | ${ci(summary.jaccard)} |`);
output.push(`| Median / 10th percentile Jaccard | ${f3(summary.jaccardMedian)} / ${f3(summary.jaccardP10)} |`);
output.push(`| Frames with JI >= 0.90 | ${ci(summary.success90, pct)} |`);
output.push(`| Frames with JI >= ${SUCCESS} | ${ci(summary.success95, pct)} |`);
output.push(`| Mean corner error (% of page diagonal) | ${ci(summary.cornerErrorMean, pct)} |`);
output.push(`| Worst-corner error, 95th percentile | ${pct(summary.cornerErrorMaxP95)} |`);
output.push(`| Reported "found" | ${pct(summary.foundRate)} |`);
output.push(`| Confidence AUROC (JI >= ${SUCCESS} vs below) | ${f3(summary.confidenceAuroc)} |`);
output.push(`| Detect time p50 / p95 | ${summary.timeP50.toFixed(1)} / ${summary.timeP95.toFixed(1)} ms |`);
if (summary.unreadable) output.push(`| Unreadable images (scored 0) | ${summary.unreadable} |`);

const byKey = new Map();
for (const row of rows) byKey.set(row[options.by], [...(byKey.get(row[options.by]) ?? []), row]);
output.push('', `## By ${options.by}`, '', `| ${options.by} | Frames | Mean JI [95 % CI] | JI >= ${SUCCESS} |`, '| --- | --- | --- | --- |');
const perGroup = {};
for (const [key, groupRows] of [...byKey].sort()) {
  const s = summarize(groupRows);
  perGroup[key] = s;
  output.push(`| ${key} | ${s.frames} | ${ci(s.jaccard)} | ${pct(s.success95.mean)} |`);
}

const worst = Number(options.worst);
if (worst > 0) {
  // Error analysis starts here: open these with `docscan-cli <image> out.png --debug outline.jpg`.
  output.push('', `## ${worst} worst frames`, '', '| Image | JI | Found | Confidence |', '| --- | --- | --- | --- |');
  for (const row of [...rows].sort((a, b) => a.jaccard - b.jaccard).slice(0, worst)) {
    output.push(`| ${row.image} | ${f3(row.jaccard)} | ${row.found ? 'yes' : 'no'} | ${f3(row.confidence)} |`);
  }
}

let comparison;
if (options.baseline) {
  const key = (r) => `${r.group}|${r.image}`;
  const baseline = new Map(readResults(options.baseline).map((r) => [key(r), r]));
  const paired = rows.filter((r) => baseline.has(key(r))).map((r) => ({ ...r, delta: r.jaccard - baseline.get(key(r)).jaccard }));
  const delta = meanWithCi(paired, (r) => r.delta);
  const better = paired.filter((r) => r.delta > 0.01).length;
  const worse = paired.filter((r) => r.delta < -0.01).length;
  const verdict =
    delta.low > 0 ? 'significant improvement' : delta.high < 0 ? 'significant regression' : 'no significant difference';
  comparison = { baseline: options.baseline, paired: paired.length, delta, better, worse, verdict };
  output.push('', `## Compared with ${options.baseline} (paired by image)`, '');
  output.push(`- ${paired.length} common frames`);
  output.push(`- Mean JI change: ${ci(delta)} -> **${verdict}** at 95 %`);
  output.push(`- Frames improved / worsened by more than 0.01 JI: ${better} / ${worse}`);
}

console.log(output.join('\n'));
if (options.json) {
  writeFileSync(options.json, JSON.stringify({ results: positionals[0], summary, [`by_${options.by}`]: perGroup, comparison }, null, 2));
}
