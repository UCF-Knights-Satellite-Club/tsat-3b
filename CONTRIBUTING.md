# Contributing to tsat-3b

## Where things go

```
FlightController/                 flight software (Arduino sketch)
PowerDistributionBoard/<design>/  one folder per power board design
MainBoard/                        flight computer board
```

Each board folder looks like this:

```
<design>/
  README.md        what the board is, its status, power tree, connectors
  *.kicad_pro, *.kicad_sch, *.kicad_pcb, custom libraries
  docs/            schematic PDF, board renders
  bom/             BOM.csv
  fabrication/     Gerbers + drill files (and optionally a zip of them for the board house)
```

## Rules

1. **Always upload the KiCad project with its Gerbers.** Gerbers on their own can't be reviewed, checked or edited.
2. **Don't upload a zip of a whole project.** Upload the files themselves so GitHub can show what changed. A zip of the Gerbers
   inside `fabrication/` is fine.
3. **Don't upload KiCad's local files:** `*.kicad_prl`, `*.lck`, `*-backups/`, `.history/`, `fp-info-cache`. The `.gitignore`
   already skips them if you use git.
4. **Update the board in place.** When you revise a board, overwrite its files in the same folder; git keeps every old version.
   Only make a new folder for a genuinely different design.
5. **Write one sentence in the PR description** saying what changed and why, e.g. "Rev 1.1: swap J1 to XT30 for battery current."
6. **Ask someone to review before you merge**, especially before ordering boards.

## Uploading through the GitHub website

1. Open the folder you want on GitHub (for example `PowerDistributionBoard/1.5U-Rev1/`).
2. Click **Add file → Upload files** and drag the files in. You can drag a whole folder (for example `fabrication/`) and its
   structure is kept.
3. To create a new folder, use **Add file → Create new file** and type the name with a slash at the end, like `fabrication/`.
   Without the slash you get an empty file instead of a folder.
4. Choose **Create a new branch** and **Propose changes**, then open the pull request.
