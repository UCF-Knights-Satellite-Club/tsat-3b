# Power distribution board

The power board takes the 3S LiPo and supplies every other part of TSat-3B: the flight computer, the camera, both steering servos
and the nichrome cutter. Three designs have been made. **Only `1.5U-Rev1` is current.** The other two are kept for reference;
don't order them.

| Design | Size | Regulators | Status |
|---|---|---|---|
| [`1.5U-Rev1/`](1.5U-Rev1/) | 70 × 70 mm, 2-layer | Off-board modules: XL4015 (7.4 V servos), MP1584 (5 V logic) | **Current**, Rev 1.0 |
| [`TPSM63610-draft/`](TPSM63610-draft/) | 70 × 67 mm, 2-layer | On-board TI TPSM63610 × 2 (5.0 V, 7.2 V) | Superseded |
| [`RevB-SY8105/`](RevB-SY8105/) | 55 × 40 mm, 2-layer | On-board Silergy SY8105 × 2 (5 V logic, ~6 V servo) | Superseded |

All three are KiCad 10 projects. Open the `.kicad_pro` file in each folder.
