"""
Plot a flight log as an SVG (standard library only, no installs needed).

Works with the CSV from tools/pc_sim.cpp, and with telemetry captured from the
ESP32's Serial port (lines starting with '#' are skipped).

    python tools/plot_sim.py run3.csv            -> writes run3.svg next to it
    python tools/plot_sim.py run3.csv out.svg

Top: ground track (north up) with the mission path, the zig-zag corridor and
the target. Bottom: height and spare height ("excess") against time.
Track colour shows the flight phase.
"""
import csv
import math
import os
import re
import sys

PHASE_COLORS = {
    "DEPLOY": "#9ca3af", "NAVIGATE": "#2563eb", "ZIGZAG_BACK": "#d97706",
    "ZIGZAG_FWD": "#f59e0b", "FINAL": "#16a34a", "ORBIT": "#9333ea",
    "SPIRAL": "#dc2626", "FLARE": "#111827", "LANDED": "#111827",
}
M_PER_DEG_LAT = 111195.0


def read_mission(sketch_dir):
    """Pull TARGET, PATH and ZIGZAG_LEG out of Mission.h so the plot matches the code."""
    try:
        text = open(os.path.join(sketch_dir, "Mission.h"), encoding="utf-8").read()
    except OSError:
        return None
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    num = r"(-?\d+(?:\.\d+)?)"
    t = re.search(r"TARGET\s*=\s*\{\s*" + num + r"\s*,\s*" + num, text)
    p = re.search(r"PATH\[\]\s*=\s*\{(.*?)\};", text, flags=re.S)
    z = re.search(r"ZIGZAG_LEG\s*=\s*(-?\d+)", text)
    if not (t and p):
        return None
    lat0, lon0 = float(t.group(1)), float(t.group(2))
    m_lon = M_PER_DEG_LAT * math.cos(math.radians(lat0))
    pts = [((float(a) - lat0) * M_PER_DEG_LAT, (float(b) - lon0) * m_lon)
           for a, b in re.findall(r"\{\s*" + num + r"\s*,\s*" + num + r"\s*\}", p.group(1))]
    return {"path": pts, "zigzag": int(z.group(1)) if z else -1}


def load(path):
    with open(path, newline="") as f:
        lines = [l for l in f if l.strip() and not l.startswith("#")]
    rows = list(csv.DictReader(lines))
    if not rows:
        sys.exit("no data rows in " + path)
    # pc_sim logs the true position too; the ESP32 telemetry has only the estimate
    n_key = "true_n" if "true_n" in rows[0] else "n"
    e_key = "true_e" if "true_e" in rows[0] else "e"
    h_key = "true_h" if "true_h" in rows[0] else "h"
    out = []
    for r in rows:
        try:
            out.append({"t": float(r["t"]), "phase": r["phase"], "n": float(r[n_key]),
                        "e": float(r[e_key]), "h": float(r[h_key]), "excess": float(r.get("excess", 0) or 0)})
        except (KeyError, ValueError):
            continue
    return out


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(src)[0] + ".svg"
    rows = [r for r in load(src) if r["phase"] not in ("PREFLIGHT", "ARMED")]
    mission = read_mission(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

    W, MAP_H, CH_H, PAD = 760, 560, 220, 40
    pts = [(r["n"], r["e"]) for r in rows] + [(0.0, 0.0)]
    if mission:
        pts += mission["path"]
    n_lo, n_hi = min(p[0] for p in pts) - 20, max(p[0] for p in pts) + 20
    e_lo, e_hi = min(p[1] for p in pts) - 20, max(p[1] for p in pts) + 20
    scale = min((W - 2 * PAD) / (e_hi - e_lo), (MAP_H - 2 * PAD) / (n_hi - n_lo))

    def xy(n, e):
        return PAD + (e - e_lo) * scale, MAP_H - PAD - (n - n_lo) * scale

    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{MAP_H + CH_H + 30}" '
           f'font-family="sans-serif" font-size="12">',
           f'<rect width="100%" height="100%" fill="white"/>']

    # 10 m grid
    step = 50 if (e_hi - e_lo) > 300 else 20
    for v in range(int(math.floor(e_lo / step) * step), int(e_hi) + 1, step):
        x, _ = xy(0, v)
        svg.append(f'<line x1="{x:.1f}" y1="{PAD}" x2="{x:.1f}" y2="{MAP_H - PAD}" stroke="#eee"/>')
    for v in range(int(math.floor(n_lo / step) * step), int(n_hi) + 1, step):
        _, y = xy(v, 0)
        svg.append(f'<line x1="{PAD}" y1="{y:.1f}" x2="{W - PAD}" y2="{y:.1f}" stroke="#eee"/>')

    if mission:
        path = mission["path"] + [(0.0, 0.0)]
        d = " ".join(f"{xy(n, e)[0]:.1f},{xy(n, e)[1]:.1f}" for n, e in path)
        svg.append(f'<polyline points="{d}" fill="none" stroke="#94a3b8" stroke-width="2" stroke-dasharray="6 4"/>')
        z = mission["zigzag"]
        if 0 <= z < len(mission["path"]) - 1:
            (x1, y1), (x2, y2) = xy(*mission["path"][z]), xy(*mission["path"][z + 1])
            svg.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" '
                       f'stroke="#fcd34d" stroke-width="10" stroke-opacity="0.5"/>')
        for i, (n, e) in enumerate(mission["path"]):
            x, y = xy(n, e)
            svg.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="4" fill="#64748b"/>'
                       f'<text x="{x + 6:.1f}" y="{y - 6:.1f}" fill="#475569">PATH[{i}]</text>')

    # track, one polyline per phase segment
    seg, cur = [], None
    def flush():
        if len(seg) > 1:
            d = " ".join(f"{x:.1f},{y:.1f}" for x, y in seg)
            svg.append(f'<polyline points="{d}" fill="none" stroke="{PHASE_COLORS.get(cur, "#000")}" stroke-width="2"/>')
    for r in rows:
        p = xy(r["n"], r["e"])
        if r["phase"] != cur:
            flush()
            seg = seg[-1:] if seg else []
            cur = r["phase"]
        seg.append(p)
    flush()

    tx, ty = xy(0, 0)
    svg.append(f'<circle cx="{tx:.1f}" cy="{ty:.1f}" r="7" fill="none" stroke="#dc2626" stroke-width="2"/>'
               f'<circle cx="{tx:.1f}" cy="{ty:.1f}" r="2" fill="#dc2626"/>')
    lx, ly = xy(rows[-1]["n"], rows[-1]["e"])
    miss = math.hypot(rows[-1]["n"], rows[-1]["e"])
    svg.append(f'<text x="{lx + 8:.1f}" y="{ly + 4:.1f}" font-weight="bold">landed {miss:.1f} m from target</text>')
    svg.append(f'<text x="{PAD}" y="20" font-size="14" font-weight="bold">{os.path.basename(src)}  (north up, grid {step} m)</text>')

    # legend
    lx = PAD
    for name, col in PHASE_COLORS.items():
        if name == "LANDED":
            continue
        svg.append(f'<rect x="{lx}" y="{MAP_H - 18}" width="12" height="4" fill="{col}"/>'
                   f'<text x="{lx + 16}" y="{MAP_H - 13}">{name}</text>')
        lx += 16 + 8 * len(name) + 12

    # height / excess chart
    top = MAP_H + 10
    t0, t1 = rows[0]["t"], rows[-1]["t"]
    h_hi = max(r["h"] for r in rows) * 1.05 + 1
    ex_lo = min(min(r["excess"] for r in rows), 0)
    def cx(t):
        return PAD + (t - t0) / max(t1 - t0, 1e-6) * (W - 2 * PAD)
    def cy(v):
        return top + CH_H - 20 - (v - ex_lo) / (h_hi - ex_lo) * (CH_H - 30)
    svg.append(f'<line x1="{PAD}" y1="{cy(0):.1f}" x2="{W - PAD}" y2="{cy(0):.1f}" stroke="#ccc"/>')
    svg.append('<polyline points="' + " ".join(f"{cx(r['t']):.1f},{cy(r['h']):.1f}" for r in rows) +
               '" fill="none" stroke="#2563eb" stroke-width="2"/>')
    svg.append('<polyline points="' + " ".join(f"{cx(r['t']):.1f},{cy(r['excess']):.1f}" for r in rows) +
               '" fill="none" stroke="#d97706" stroke-width="1.5"/>')
    svg.append(f'<text x="{PAD}" y="{top + 12}"><tspan fill="#2563eb">height (m)</tspan>   '
               f'<tspan fill="#d97706">spare height (m)</tspan>   {t0:.0f}-{t1:.0f} s</text>')
    svg.append("</svg>")

    with open(dst, "w", encoding="utf-8") as f:
        f.write("\n".join(svg))
    print("wrote", dst)


if __name__ == "__main__":
    main()
