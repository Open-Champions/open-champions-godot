"""Step-by-step journey comparison: step/result pairs, then every differing field."""
import json, sys
HOST = {"fps", "memory_bytes", "swing_wall_msec"}
def load(p):
    return json.load(open(p))
def flat(v, pre=""):
    out = {}
    if isinstance(v, dict):
        for k, x in v.items():
            out.update(flat(x, f"{pre}.{k}" if pre else k))
    elif isinstance(v, list) and v and all(isinstance(x, dict) for x in v):
        for i, x in enumerate(v):
            out.update(flat(x, f"{pre}[{i}]"))
    else:
        out[pre] = v
    return out
a, b = load(sys.argv[1]), load(sys.argv[2])
sa, sb = a["steps"], b["steps"]
print(f"steps {len(sa)} vs {len(sb)}")
pairs_same = [(x.get("step"), x.get("result")) for x in sa] == [(x.get("step"), x.get("result")) for x in sb]
print("step/result pairs identical:", pairs_same)
for i in range(max(len(sa), len(sb))):
    x = sa[i] if i < len(sa) else {}
    y = sb[i] if i < len(sb) else {}
    fx, fy = flat(x), flat(y)
    diffs = [k for k in sorted(set(fx) | set(fy)) if fx.get(k) != fy.get(k) and k.split(".")[-1] not in HOST]
    if (x.get("step"), x.get("result")) != (y.get("step"), y.get("result")) or diffs:
        print(f"[{i}] {x.get('step')}={x.get('result')} | {y.get('step')}={y.get('result')}")
        for k in diffs:
            print(f"    {k}: {json.dumps(fx.get(k))[:160]} -> {json.dumps(fy.get(k))[:160]}")
ra, rb = a.get("refusals", []), b.get("refusals", [])
print("refusal kinds", len(ra), "->", len(rb))
sra = set(json.dumps(r, sort_keys=True) for r in ra); srb = set(json.dumps(r, sort_keys=True) for r in rb)
for r in sorted(sra - srb): print("  only before:", r[:220])
for r in sorted(srb - sra): print("  only after:", r[:220])
