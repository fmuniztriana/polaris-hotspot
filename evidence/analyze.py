#!/usr/bin/env python3
"""Run on capture host to parse reference local timestamps without assuming timezone."""
import csv
import datetime
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent

def correlation(a, b):
    ma, mb = sum(a)/len(a), sum(b)/len(b)
    numerator = sum((x-ma)*(y-mb) for x, y in zip(a,b))
    denominator = math.sqrt(sum((x-ma)**2 for x in a)*sum((y-mb)**2 for y in b))
    return numerator/denominator if denominator else None

def metrics(a, b):
    differences = [x-y for x,y in zip(a,b)]
    return {'mae_C': sum(abs(d) for d in differences)/len(differences),
            'bias_C': sum(differences)/len(differences),
            'max_abs_error_C': max(abs(d) for d in differences),
            'pearson': correlation(a,b)}

def main():
    samples = [json.loads(s) for s in (ROOT/'samples.jsonl').read_text().splitlines() if s.strip()]
    with (ROOT/'reference.csv').open() as f:
        reader = csv.DictReader(f, skipinitialspace=True)
        rows = []
        for line in reader:
            try:
                stamp = datetime.datetime.strptime(line['timestamp'].strip(), '%a %d %b %Y %H:%M:%S.%f')
                epoch = stamp.replace(tzinfo=datetime.timezone(datetime.timedelta(hours=-2))).timestamp()
                rows.append({'epoch_s': epoch, 'edge_C': float(line['edge_c']),
                             'hotspot_C': float(line['hotspot_c']),
                             'busy_gfx': float(line['busy_gfx_pct'])})
            except (ValueError, KeyError, TypeError):
                continue
    pairs = []
    for sample in samples:
        nearest = min(rows, key=lambda r: abs(r['epoch_s']-sample['epoch_s']))
        distance = abs(nearest['epoch_s']-sample['epoch_s'])
        if distance <= .040:
            pair = dict(sample)
            pair.update(nearest)
            pair['sample_epoch_s'] = sample['epoch_s']
            pair['distance_ms'] = distance*1000
            pairs.append(pair)
    if not pairs:
        raise RuntimeError('No timestamp pairs within 40 ms; inspect CSV/timezone')
    raw = [p['asic_max_temp_C'] for p in pairs]
    ctf = [p['ctf_temp_C'] for p in pairs]
    edge = [p['edge_C'] for p in pairs]
    hotspot = [p['hotspot_C'] for p in pairs]
    result = {'samples': len(samples), 'csv_rows': len(rows), 'pairs_within_40ms': len(pairs),
              'ASIC_MAX_vs_reference_hotspot': metrics(raw,hotspot),
              'ASIC_MAX_vs_reference_edge': metrics(raw,edge),
              'CTF_vs_reference_edge': metrics(ctf,edge),
              'CTF_vs_reference_hotspot': metrics(ctf,hotspot),
              'ASIC_MAX_range_C': [min(raw),max(raw)],
              'CTF_range_C': [min(ctf),max(ctf)],
              'reference_hotspot_range_C': [min(r['hotspot_C'] for r in rows),max(r['hotspot_C'] for r in rows)],
              'reference_edge_range_C': [min(r['edge_C'] for r in rows),max(r['edge_C'] for r in rows)],
              'max_reference_hotspot_edge_delta_C': max(r['hotspot_C']-r['edge_C'] for r in rows),
              'max_gfx_busy_pct': max(r['busy_gfx'] for r in rows),
              'pause_ms_mean': sum(s['pause_ms'] for s in samples)/len(samples),
              'pause_ms_max': max(s['pause_ms'] for s in samples),
              'max_pair_distance_ms': max(p['distance_ms'] for p in pairs),
              'all_restored': all(s['selector_restored'] for s in samples),
              'example_peak_pair': max(pairs,key=lambda p:p['hotspot_C'])}
    loaded = [p for p in pairs if p['busy_gfx'] >= 90]
    result['pairs_gfx_busy_at_least_90pct'] = len(loaded)
    if loaded:
        result['loaded_ASIC_MAX_vs_hotspot'] = metrics([p['asic_max_temp_C'] for p in loaded], [p['hotspot_C'] for p in loaded])
        result['loaded_ASIC_MAX_vs_edge'] = metrics([p['asic_max_temp_C'] for p in loaded], [p['edge_C'] for p in loaded])
    result['pairs_error_at_most_0_5C'] = sum(abs(p['asic_max_temp_C']-p['hotspot_C']) <= .5 for p in pairs)
    result['pairs_error_at_most_1C'] = sum(abs(p['asic_max_temp_C']-p['hotspot_C']) <= 1 for p in pairs)
    (ROOT/'paired-samples.json').write_text(json.dumps(pairs,indent=2))
    (ROOT/'comparison.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))

if __name__ == '__main__':
    main()
