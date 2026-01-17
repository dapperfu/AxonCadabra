---
name: axon-ble-app-analysis-and-hardening
overview: Analyze AxonCadabra's Android BLE scan/advertise/fuzz implementation with emphasis on security/privacy, BLE correctness, stability/perf, and code quality; produce a written report and an actionable refactor/fix plan (no execution yet).
todos:
  - id: persist-plan
    content: Save this plan to `plans/` at repo root (tracked in git).
    status: completed
  - id: inventory-entrypoints
    content: Inventory app entrypoints, permissions, and exported components; map runtime-permission and Bluetooth-enable flows.
    status: in_progress
  - id: analyze-ble-scan
    content: Analyze BLE scanning path for correctness/perf (filters, threading, lifecycle) and identify failure modes across API 26–34.
    status: pending
  - id: analyze-ble-advertise
    content: Analyze BLE advertising path for correctness/perf (payload size limits, connectable behavior, restart cadence, callbacks) and identify failure modes across API 26–34.
    status: pending
  - id: fuzz-mode-design
    content: Define fuzz-mode behavior for both sequential and randomized strategies; document expected UX and safety guardrails (rate, stop conditions, lifecycle).
    status: pending
  - id: security-privacy-hardening
    content: Identify security/privacy issues and propose mitigations (disable backup, reduce logs, avoid leaking MAC/payload data, minimize exported surfaces).
    status: pending
  - id: architecture-refactor-plan
    content: Propose a refactor that separates BLE concerns from UI (e.g., a `BleController` class + lifecycle-safe state), improving testability and readability.
    status: pending
  - id: deliver-report
    content: "Deliver a prioritized written report: findings, severity, rationale, and recommended fixes with file-level pointers."
    status: pending
  - id: deliver-implementation-plan
    content: Deliver a concrete implementation plan (file-by-file steps) for the agreed fixes/refactor, including a minimal test/verification checklist.
    status: pending
---

### Goals

- Produce **(1) a written analysis report** and **(2) an implementation plan** focused on **security/privacy**, **BLE correctness**, **stability/performance**, and **architecture/code quality**.
- Target **Android API 26–34**.
- Respect your preferences: **keep current permissions for reliability**, **disable Android backup**, **minimize logs everywhere**, **prompt to enable Bluetooth**, **use `ScanFilter` when possible**, **keep connectable advertising**, and **support both sequential and randomized fuzz modes**.

### What I've found so far (current shape)

- The app logic is concentrated in a single activity: [`app/src/main/java/com/axon/blecontroller/MainActivity.kt`](/projects/vibe_/AxonCadabra/app/src/main/java/com/axon/blecontroller/MainActivity.kt).
  - Scanning: `startScan(null, scanSettings, scanCallback)` then filters by MAC prefix `TARGET_OUI = "00:25:DF"`.
  - Advertising: broadcasts service data under UUID `0000FE6C-0000-1000-8000-00805F9B34FB` and optionally "fuzzes" bytes by repeatedly stopping/starting advertising every 500ms.
- Manifest currently sets `android:allowBackup="true"` and exports only the launcher activity: [`app/src/main/AndroidManifest.xml`](/projects/vibe_/AxonCadabra/app/src/main/AndroidManifest.xml).

### Analysis approach (how I'll structure the report)

- **Entry points & permissions**
  - Review [`app/src/main/AndroidManifest.xml`](/projects/vibe_/AxonCadabra/app/src/main/AndroidManifest.xml) and runtime permission gating in `MainActivity`.
  - Map expected behavior when permissions are denied, Bluetooth is off, BLE advertiser is unavailable, and on lifecycle transitions.
- **BLE scan path**
  - Recommend adding a `ScanFilter` based on the most reliable available discriminator (likely service UUID `0xFE6C`), while retaining the OUI check as a secondary filter.
  - Evaluate power/perf implications of `SCAN_MODE_LOW_LATENCY` and unfiltered scans.
- **BLE advertise path**
  - Validate advertising payload sizing for service-data and scan response across devices.
  - Evaluate stability of repeated stop/start at 500ms (vendor stacks can be sensitive), and propose guardrails/backoff.
- **Fuzz mode**
  - Specify sequential + randomized fuzz behaviors and how to switch modes in the UI.
  - Ensure fuzz loop is lifecycle-safe and doesn't thrash the advertiser unnecessarily.
- **Security/privacy**
  - Propose `allowBackup=false` and a logging strategy consistent with "minimize logs everywhere".
  - Ensure no sensitive identifiers (MACs, payload bytes) are exposed unnecessarily.
- **Architecture**
  - Propose extraction of BLE operations from `MainActivity` into a focused component (e.g., `BleController`), with a small state model and clean UI bindings.

### Proposed high-level flow (current → refactored target)

```mermaid
flowchart TD
  UI[MainActivity_UI] --> Permissions[PermissionGate]
  UI --> BluetoothState[BluetoothStateGate]
  Permissions --> BleController[BleController]
  BluetoothState --> BleController
  BleController --> Scanner[LeScanner]
  BleController --> Advertiser[LeAdvertiser]
  BleController --> FuzzLoop[FuzzScheduler]
  Scanner --> DeviceList[FoundDevicesModel]
  DeviceList --> UI
```

### Files likely touched (once you approve execution later)

- [`app/src/main/java/com/axon/blecontroller/MainActivity.kt`](/projects/vibe_/AxonCadabra/app/src/main/java/com/axon/blecontroller/MainActivity.kt)
- [`app/src/main/AndroidManifest.xml`](/projects/vibe_/AxonCadabra/app/src/main/AndroidManifest.xml)
- Potentially new files under [`app/src/main/java/com/axon/blecontroller/`](/projects/vibe_/AxonCadabra/app/src/main/java/com/axon/blecontroller/) for extracted BLE/controller logic (only after you approve execution).

### Verification checklist (for the implementation plan)

- On API 26–30: scan works with location permission; advertise works where supported; denial states are handled.
- On API 31–34: scan/advertise/connect permissions are requested and enforced correctly.
- Bluetooth OFF: user gets a prompt to enable; app doesn't crash.
- Advertiser unsupported: app degrades gracefully; fuzz doesn't start.
- Fuzz sequential/random: start/stop is stable; no runaway handler callbacks after lifecycle events.
- Logs: no MAC/payload dumps in normal operation (per your preference).
