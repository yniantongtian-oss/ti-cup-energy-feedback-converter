# Hardware validation evidence

This directory is reserved for **real measurement evidence**. It is intentionally documentation-only until a physical prototype is tested.

Recommended layout:

```text
hardware/validation/
  manifests/
  raw/
    csv/
    oscilloscope/
  calibration/
  reports/
```

Each test record should identify:

- date and test purpose;
- board/schematic/PCB revision;
- BOM revision;
- firmware commit SHA;
- supply, load and current-limit settings;
- instrument manufacturer/model and relevant range;
- raw files and SHA-256 digests;
- environmental conditions when relevant;
- pass/fail criterion and result;
- operator notes and anomalies.

Never commit fabricated measurements. If large binary oscilloscope captures are stored outside Git, commit a manifest containing their immutable location and checksum.
