# RX 550 evidence

PCI 1002:699f, revision C7, subsystem 1002:0b04; recorded 2026-10-27.

158 register samples, 158 confirmed selector restorations, 600 reference telemetry rows, 150 matched pairs. The same register and conversion as RX580 2048SP were used. Full test conditions and results are in the repository README.

`reference.csv` is a projection of the original reference telemetry retaining only timestamps, edge/hotspot temperatures and graphics activity. Measured values are unchanged; timestamps are standardized to UTC. Tool identifiers and unrelated fields are intentionally omitted; complete original logs remain in the private bench archive.

`samples.jsonl` contains the register words and restoration flags. `paired-samples.json` and `comparison.json` contain the original analysis with neutral reference labels. `run-result.json` records the supervisor outcome. `analyze.py` reproduces the pairing and metrics using UTC reference timestamps. `SHA256SUMS.txt` covers this public dataset.
