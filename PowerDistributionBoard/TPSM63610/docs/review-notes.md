# TSAT-3B-PDB review

Reviewed 2026-11-14. **Final native KiCad 10.0.8: 0 DRC errors/warnings, 0 unconnected items, 0 schematic parity issues, and 0 ERC errors/warnings.** A real KiCad 10.0.8 binary was executed; these are not inferred Python-only results. `Review/DRC.json` and `Review/ERC.json` are the native evidence.

## Scope and preserved layout

Inputs were the newly uploaded PCB `TSAT3B_PDB_RevL(20261115-033543).kicad_pcb`, schematic `(4)`, project `(5)`, and `BOM_USA.csv`. Required local libraries came from the previously delivered Rev R package, then were checked against the uploaded schematic/PCB by native ERC/DRC. The input lock and local-view files were excluded. Primary files and their internal project/sheet links now use **TSAT-3B-PDB**. Libraries are organized under `Libraries/`; their logical aliases are retained so no component is replaced.

The whole file was inspected: **74 footprints, 408 segments, 56 vias, one B.Cu GND zone**. The exact seven footprints after D6 are **H4, C35, D1, JP1, Q2, C17 and J9**. All 74 existing footprint coordinates, rotations and UUIDs, all 408 track centerlines/UUIDs, every original pad's geometry/net/UUID and all vias are preserved. There are **70 fitted electrical components**, unchanged.

## Applied fixes

1. The input already had zero disconnected pads and no dangling copper. No reconnection or rerouting was necessary.
2. Two existing segments in the shared servo supply trunk were **1.0 mm** wide. They are now **1.5 mm**, with identical start/end coordinates and UUIDs: `19e05d54-6181-4458-bace-69950cb008e1` and `6b17866f-af01-4e16-b77d-a6188dfcf87a`. The main feed now has an F.Cu path using tracks at least 1.5 mm wide from U2 to F3/F4/F5, C15/C35/C36 and J9. The separate fused output paths remain 1.5 mm to J7 and 2.0 mm to J8. Smaller low-current bias/feedback spurs on VSERVO_7V2 remain in place; an entire-net minimum is not the same as its load-current trunk width.
3. J9's printed **1/2** labels had been deleted. They are restored on F.SilkS to the right of their pads, centered at **(157.6,106.6)** for 1 and **(157.6,98.6)** for 2. Conservative text bounding boxes clear front copper by **0.425 mm**. No footprint was moved.
4. The BOM already matches the schematic and PCB: **40 grouped rows, 70 components**, correct quantities and MPNs. Only references to the renamed wiring guide were updated; component purchasing data and values are unchanged.

`TSAT-3B-PDB.patch` is the exact unified diff, using the final canonical filenames. It includes PCB edits, the BOM guide-link rename, and project/library path updates. `Review/Changes.json` records hashes and patch verification. No physical components were added.

## Moved parts and Rev R verification

- **C15:** (151.2,75.3), −90°; pin 1 VSERVO_7V2, pin 2 GND. **R24:** (146.3,79.075), 90°; the actual pad coordinates are **pin 1 BRAKE_OUT at (146.3,79.90)** and **pin 2 BRAKE_SENSE at (146.3,78.25)**. The coordinates in the written request were reversed for this rotation; the actual board and schematic agree, so the resistor was not rotated or remapped.
- **F4:** (143.1,100), −90°; pin 1 VSERVO_7V2, pin 2 SERVO2_V. **J7:** (140.7,109.715), 180°; **J8:** (124,109.6), 180°; each is pin 1 GND, pin 2 fused servo voltage, pin 3 shared D. All pads on these moved components are connected.
- H1 (94,49.9) and H2 (94,108.3) are retained, along with all other placements. Courtyard checks cover every footprint and find **zero overlaps**.
- Removed Section 02 orphan items remain absent. VBAT_RAW, the servo symbol pin names, J3/J7/J8 associations and pin metadata, and the Rev R servo/clamp schematic revision are retained. Native schematic parity and library-symbol comparison pass.
- R18 **TNPW060321K5BEEA**, R24 **RC0603FR-07806KL**, R6 **CRCW060322K0FKEA**, C15/C35/C36 **T521D227M016ATE025**, and J7/J8 **0436500315** are confirmed in the actual BOM and CAD.
- Nominal electrical values are retained: 5.016 V / 7.200 V rails, 15.8 kΩ frequency resistors, FPWM MODE=VCC, RBOOT–CBOOT links, and 100 kΩ/22 kΩ battery sensing. The nominal 12.6 V and 9.3 V sense outputs are 2.272 V and 1.677 V. Clamp resistor/capacitor changes are intact; calculated thresholds are nominal component-model values rather than measured trip limits.

## Remaining seven footprints

| Ref | Position, rotation | Courtyard bounds (mm) |
|---|---|---|
| Q2 | (103.5, 58.5), 180° | X 99.8–107.2; Y 55.8–61.2 |
| D1 | (101, 52.5), 180° | X 98.65–103.35; Y 51.35–53.65 |
| JP1 | (114, 52.5), 90° | X 112.23–118.32; Y 50.73–54.27 |
| J9 | (154.4, 106.6), 90° | X 151.6–157.2; Y 95.8–109.4 |
| C17 | (143, 54), 0° | X 138.59–147.41; Y 51.6–56.4 |
| C35 | (133.6, 80.5), -90° | X 131.2–136; Y 76.09–84.91 |
| H4 | (153.5, 49.95), 0° | X 151.15–155.85; Y 47.6–52.3 |

Pad mapping: **Q2** 1–3 MAIN_SOURCES, 4 Net-(D1-A), 5–8 Net-(F1-Pad2); **D1** 1/K MAIN_SOURCES, 2/A Net-(D1-A); **JP1** 1 Net-(JP1-A), 2 GND; **J9** 1 VSERVO_7V2, 2 BRAKE_DRAIN; **C17** 1 VBUS, 2 GND; **C35** 1 VSERVO_7V2, 2 GND. **H4** is a board-only 2.10 mm NPTH with no net. J9 pad 2 is a switched brake node, not GND. Full selected pad coordinates and silkscreen records are in `Review/Geometry.json`.

## Connectivity, clearances and ground returns

The independent geometry graph covers copper pads, every segment, plated vias, both copper layers and the freshly filled polygon. All **34 active nets** have one connected pad group; seven intentional no-connect nets are separate by design. There are **0 padless copper groups, 0 dangling track endpoints and 0 vias missing a layer connection**. R24.1 connects to U4.1/R22.1 on F.Cu; R24.2 connects to R18.2/R19.1 on F.Cu and U4.4 through the existing B.Cu sense branch.

Board outline: **70 × 67 mm**, nominal thickness **1.6 mm**, **two copper layers**. Minimum modeled copper-edge gaps are pads **2.400 mm**, tracks **2.590 mm**, vias **4.000 mm**, filled GND zone **0.5005 mm**. All meet the configured 0.50 mm edge rule. Minimum track width is **0.20 mm**, above the 0.16 mm constraint; overall maximum is 2.00 mm. No native 0.20 mm copper-clearance violations were found. Python minima round to ~0.200 mm on both layers; finite polygon approximation/contact tolerance is recorded in `Review/Geometry.json`.

All **56 vias** are **0.60 mm diameter / 0.30 mm drill**, giving nominal **0.15 mm annular ring**; **41 are GND vias**. No 0.30 mm hole/copper or 0.25 mm hole/hole violations were found; minimum hole-edge gap is **0.970 mm**.

The B.Cu ground fill is **one polygon and one connected component**, approximately **4018.49 mm²**, with no separate filled islands. U1/U2 AGND/PGND, Q1/Q4 sources, capacitor negative pads (including C15/C35/C36 and C25–C27), J7/J8 ground pins and J1.2 are in the same ground network. The 1.02 mm plated servo holes and 1.27 mm NPTH pegs do not split the plane. The graph preserves a connection from both servo ground pads and source via anchors to J1.2 after a 0.24 mm plane erosion, consistent with the 0.5 mm thermal spokes rather than a disconnected sliver.

Q1's three nearby source escape vias and Q4's two nearby added source vias remain; their nearest top-side escapes are about **1.25 mm × 1.0 mm**. This demonstrates short, stitched geometric returns, not a field-solved impedance or thermal guarantee. The morphology model fills plated pad/via centers as graph anchors; native annular/drill rules are checked independently.

## Power-net copper summary

| Net | Min–max track width (mm) | Segments | Vias |
|---|---:|---:|---:|
| +5V | 0.50–2.00 | 41 | 0 |
| ADAPTER_7V2 | 1.50–1.50 | 2 | 0 |
| BRAKE_DRAIN | 0.80–1.50 | 8 | 0 |
| CUT_PWR | 2.00–2.00 | 2 | 0 |
| GND | 0.20–1.00 | 118 | 41 |
| HEATER_LOW | 0.80–2.00 | 10 | 0 |
| HEATER_PLUS | 2.00–2.00 | 4 | 0 |
| MAIN_SOURCES | 0.50–1.50 | 10 | 2 |
| Net-(F1-Pad2) | 1.50–1.50 | 5 | 0 |
| SERVO1_V | 1.50–1.50 | 4 | 0 |
| SERVO2_V | 2.00–2.00 | 5 | 0 |
| VBAT_RAW | 1.50–1.50 | 2 | 0 |
| VBUS | 1.00–1.50 | 31 | 0 |
| VSERVO_7V2 | 0.50–1.50 | 54 | 2 |

These minima include small pin escapes and low-current branches. The principal servo load paths were separately verified at ≥1.5 mm. Complete per-net data are provided in `Review/Tracks.csv`.

## Native check settings and release scope

The exact final files pass **KiCad 10.0.8 native refilled DRC, ERC and schematic parity**. There are no individually excluded violations. The five pre-existing ignored DRC categories are unchanged: missing courtyard, track endpoint centering on vias, tuning profile geometries, footprint filters and footprint type. All courtyard geometry is additionally checked by Python. No electrical design rule was relaxed to get the pass.

This package is prepared for a final KiCad 10 **B/refill + DRC with schematic parity** and OSH Park fabrication-preview review. Open **TSAT-3B-PDB.kicad_pro** after extracting the entire ZIP. The onboard BOM is synchronized; supplier availability remains dated as listed, not newly guaranteed. Separate main/camera PCB pinouts must match the supplied wiring map. The established external brake-resistor qualification, actual cutter timing, cable polarity and current/temperature checks remain hardware tasks; a CAD pass cannot certify the assembled flight system. No extra PCB parts are needed for the edits in this review.
