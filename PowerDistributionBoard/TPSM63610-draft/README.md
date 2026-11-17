# TSAT-3B-PDB (TPSM63610 draft): superseded

> [!WARNING]
> **Superseded by [`../1.5U-Rev1/`](../1.5U-Rev1/). Don't order this board.** It's kept for reference.

70 × 67 mm, 2-layer, KiCad 10, 70 parts. Two TI TPSM63610 buck modules on the board make 5.0 V logic and 7.2 V servo power from
a 3S battery. It also has a main input switch, fused servo outputs, a nichrome cutter channel, external brake resistors, battery
voltage sensing and a 5 V output for a camera board.

- Open `TSAT-3B-PDB.kicad_pro` in KiCad 10. The symbol and footprint libraries it needs are in `Libraries/`.
- [`docs/schematic.pdf`](docs/schematic.pdf): schematic, exported with `kicad-cli`.
- [`docs/wiring.md`](docs/wiring.md): wiring notes. [`docs/review-notes.md`](docs/review-notes.md): design review log.
- [`bom/TSAT-3B-PDB_BOM.csv`](bom/TSAT-3B-PDB_BOM.csv): BOM with suppliers.

Unpacked from `TSAT-3B-PDB.zip` (uploaded in PR #2). The lock files, KiCad local settings, the `.history/` folder and the review
tool output (`Review/`, `.patch`) from that zip were left out.

Known issue if this design is ever revived: some T-joins in the schematic have no junction dot (on +5V near C3, C22, C24, C32 and R3,
on VSERVO_7V2 near C12, C13, C14 and R8, and at the Q1 gate near R12), and there are two dangling wires. KiCad can connect them
differently depending on how it reads the file.
