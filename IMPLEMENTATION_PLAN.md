# Implementation Plan: AxonCadabra Hardening & Refactoring

This document provides a concrete, file-by-file implementation plan for addressing the issues identified in the analysis report.

## Implementation Phases

### Phase 1: Critical Security & Stability Fixes (Priority 1)

**Goal:** Fix security vulnerabilities and critical stability issues without breaking existing functionality.

#### 1.1 Disable Android Backup

**File:** `app/src/main/AndroidManifest.xml`

**Changes:**
- Line 28: Change `android:allowBackup="true"` to `android:allowBackup="false"`

**Verification:**
- Build app, verify no backup-related warnings
- Test app functionality unchanged

#### 1.2 Minimize Logging

**File:** `app/src/main/java/com/axon/blecontroller/MainActivity.kt`

**Changes:**
1. Add build config check at top of class:
   ```kotlin
   private val isDebugBuild = BuildConfig.DEBUG
   ```

2. Wrap all `Log.d()` calls with debug check:
   - Line 200: Remove or guard `Log.d(TAG, ">> SCAN INITIATED FOR OUI: $TARGET_OUI")`
   - Line 212: Remove or guard `Log.d(TAG, ">> SCAN TERMINATED")`
   - Line 244: Remove or guard `Log.d(TAG, ">> TARGET ACQUIRED: $address ($deviceName)")` - **CRITICAL: Remove MAC address from logs**
   - Line 376: Remove or guard `Log.d(TAG, ">> TX INITIATED - DATA: ...")` - **CRITICAL: Remove payload data from logs**
   - Line 388: Remove or guard `Log.d(TAG, ">> TX TERMINATED")`
   - Line 401: Remove or guard `Log.d(TAG, ">> TX ACTIVE")`

3. Keep error logs (`Log.e()`) but remove sensitive data:
   - Line 251: Keep but remove any sensitive data
   - Line 418: Keep but remove any sensitive data

**Example:**
```kotlin
if (isDebugBuild) {
    Log.d(TAG, ">> SCAN INITIATED")
}
// Never log MAC addresses or payload data
```

**Verification:**
- Build release APK, verify no sensitive data in logcat
- Test app functionality unchanged

#### 1.3 Add Bluetooth Enable Prompt

**File:** `app/src/main/java/com/axon/blecontroller/MainActivity.kt`

**Changes:**
1. Add import:
   ```kotlin
   import android.content.Intent
   import android.bluetooth.BluetoothAdapter
   ```

2. Modify `initBluetooth()` (lines 140-156):
   ```kotlin
   private fun initBluetooth() {
       val bluetoothManager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
       bluetoothAdapter = bluetoothManager.adapter

       if (bluetoothAdapter == null) {
           Toast.makeText(this, ">> ERROR: NO BT ADAPTER", Toast.LENGTH_LONG).show()
           finish()
           return
       }

       // Check if Bluetooth is enabled
       if (!bluetoothAdapter!!.isEnabled) {
           val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
           startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT)
           return
       }

       bluetoothLeScanner = bluetoothAdapter?.bluetoothLeScanner
       bluetoothLeAdvertiser = bluetoothAdapter?.bluetoothLeAdvertiser

       if (bluetoothLeAdvertiser == null) {
           Toast.makeText(this, ">> ERROR: BLE ADV NOT SUPPORTED", Toast.LENGTH_LONG).show()
       }
   }
   ```

3. Add constant and result handler:
   ```kotlin
   companion object {
       // ... existing constants
       private const val REQUEST_ENABLE_BT = 1
   }

   override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
       super.onActivityResult(requestCode, resultCode, data)
       if (requestCode == REQUEST_ENABLE_BT) {
           if (resultCode == RESULT_OK) {
               // Bluetooth enabled, initialize
               bluetoothLeScanner = bluetoothAdapter?.bluetoothLeScanner
               bluetoothLeAdvertiser = bluetoothAdapter?.bluetoothLeAdvertiser
           } else {
               Toast.makeText(this, ">> ERROR: BLUETOOTH REQUIRED", Toast.LENGTH_LONG).show()
           }
       }
   }
   ```

4. Add checks before scan/advertise operations:
   - In `startScanning()` (line 182): Check `bluetoothAdapter?.isEnabled == true`
   - In `startAdvertising()` (line 336): Check `bluetoothAdapter?.isEnabled == true`

**Verification:**
- Test with Bluetooth disabled, verify prompt appears
- Test with Bluetooth enabled, verify normal operation
- Test denial of enable request, verify graceful handling

#### 1.4 Fix Lifecycle Safety in Fuzz Loop

**File:** `app/src/main/java/com/axon/blecontroller/MainActivity.kt`

**Changes:**
1. Modify `startFuzzLoop()` (lines 276-296):
   ```kotlin
   private fun startFuzzLoop() {
       fuzzRunnable = object : Runnable {
           override fun run() {
               // Check if activity is still valid
               if (isFinishing || isDestroyed) {
                   return
               }
               
               if (isFuzzing && isAdvertising) {
                   // Stop current advertising
                   stopAdvertisingInternal()

                   // Increment fuzz value and update service data
                   fuzzValue = (fuzzValue + 1) and 0xFFFF
                   updateServiceDataWithFuzz()

                   // Restart advertising with new data
                   startAdvertisingInternal()

                   // Schedule next iteration
                   fuzzHandler.postDelayed(this, FUZZ_INTERVAL_MS)
               }
           }
       }
       fuzzHandler.post(fuzzRunnable!!)
   }
   ```

2. Ensure `onDestroy()` properly cleans up (already done, but verify):
   - Line 441: `stopFuzzLoop()` is called
   - Verify `stopFuzzLoop()` removes all callbacks

**Verification:**
- Test fuzz mode, rotate device, verify no crashes
- Test fuzz mode, press back, verify cleanup
- Use memory profiler, verify no leaks

#### 1.5 Add ScanFilter for Service UUID

**File:** `app/src/main/java/com/axon/blecontroller/MainActivity.kt`

**Changes:**
1. Add import:
   ```kotlin
   import android.bluetooth.le.ScanFilter
   ```

2. Modify `startScanning()` (lines 182-204):
   ```kotlin
   private fun startScanning() {
       if (bluetoothLeScanner == null) {
           Toast.makeText(this, ">> ERROR: NO SCANNER", Toast.LENGTH_SHORT).show()
           return
       }

       // Check if Bluetooth is enabled
       if (bluetoothAdapter?.isEnabled != true) {
           Toast.makeText(this, ">> ERROR: BLUETOOTH DISABLED", Toast.LENGTH_SHORT).show()
           return
       }

       foundDevices.clear()
       deviceAdapter.notifyDataSetChanged()

       val scanSettings = ScanSettings.Builder()
           .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
           .build()

       // Add ScanFilter for service UUID
       val scanFilter = ScanFilter.Builder()
           .setServiceUuid(ParcelUuid(SERVICE_UUID))
           .build()

       try {
           bluetoothLeScanner?.startScan(listOf(scanFilter), scanSettings, scanCallback)
           isScanning = true
           scanButton.text = "[ SCAN ■ ]"
           updateStatus()
           if (isDebugBuild) {
               Log.d(TAG, ">> SCAN INITIATED")
           }
       } catch (e: SecurityException) {
           Toast.makeText(this, ">> ERROR: SCAN DENIED", Toast.LENGTH_SHORT).show()
       } catch (e: IllegalArgumentException) {
           // ScanFilter not supported on some devices, fall back to no filter
           try {
               bluetoothLeScanner?.startScan(null, scanSettings, scanCallback)
               isScanning = true
               scanButton.text = "[ SCAN ■ ]"
               updateStatus()
           } catch (e2: SecurityException) {
               Toast.makeText(this, ">> ERROR: SCAN DENIED", Toast.LENGTH_SHORT).show()
           }
       }
   }
   ```

**Verification:**
- Test scanning, verify only devices with service UUID 0xFE6C appear
- Test on devices that don't support ScanFilter, verify fallback works
- Monitor battery usage, verify improvement

---

### Phase 2: Feature Completion (Priority 2)

#### 2.1 Implement Random Fuzz Mode

**File:** `app/src/main/java/com/axon/blecontroller/MainActivity.kt`

**Changes:**
1. Add enum for fuzz mode:
   ```kotlin
   enum class FuzzMode {
       SEQUENTIAL,
       RANDOM
   }
   ```

2. Add fuzz mode state:
   ```kotlin
   private var fuzzMode: FuzzMode = FuzzMode.SEQUENTIAL
   private val recentFuzzValues = mutableSetOf<Int>() // For random mode
   ```

3. Modify `updateServiceDataWithFuzz()` (lines 303-321):
   ```kotlin
   private fun updateServiceDataWithFuzz() {
       currentServiceData = BASE_SERVICE_DATA.copyOf()

       when (fuzzMode) {
           FuzzMode.SEQUENTIAL -> {
               fuzzValue = (fuzzValue + 1) and 0xFFFF
           }
           FuzzMode.RANDOM -> {
               // Generate random value, avoid recent values
               var newValue: Int
               do {
                   newValue = (0..0xFFFF).random()
               } while (recentFuzzValues.contains(newValue) && recentFuzzValues.size < 0xFFF)
               
               fuzzValue = newValue
               recentFuzzValues.add(newValue)
               // Keep only last 100 values
               if (recentFuzzValues.size > 100) {
                   recentFuzzValues.remove(recentFuzzValues.first())
               }
           }
       }

       // Apply fuzz to service data
       currentServiceData[10] = ((fuzzValue shr 8) and 0xFF).toByte()
       currentServiceData[11] = (fuzzValue and 0xFF).toByte()
       currentServiceData[20] = ((fuzzValue shr 4) and 0xFF).toByte()
       currentServiceData[21] = ((fuzzValue shl 4) and 0xFF).toByte()

       updateFuzzStatus()
   }
   ```

4. Add UI toggle for fuzz mode (modify layout or add button):
   - Option A: Add spinner/dropdown in `activity_main.xml`
   - Option B: Long-press fuzz button to toggle mode
   - Option C: Add separate button

**Verification:**
- Test sequential mode, verify 0x0000 → 0xFFFF
- Test random mode, verify random values
- Verify no duplicate values in random mode (within recent set)

#### 2.2 Add Adapter State Monitoring

**File:** `app/src/main/java/com/axon/blecontroller/MainActivity.kt`

**Changes:**
1. Add BroadcastReceiver:
   ```kotlin
   private val bluetoothStateReceiver = object : BroadcastReceiver() {
       override fun onReceive(context: Context, intent: Intent) {
           when (intent.action) {
               BluetoothAdapter.ACTION_STATE_CHANGED -> {
                   val state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1)
                   when (state) {
                       BluetoothAdapter.STATE_OFF -> {
                           // Bluetooth disabled, stop all operations
                           if (isScanning) stopScanning()
                           if (isAdvertising) stopAdvertising()
                           if (isFuzzing) {
                               isFuzzing = false
                               stopFuzzLoop()
                           }
                           Toast.makeText(this@MainActivity, ">> BLUETOOTH DISABLED", Toast.LENGTH_LONG).show()
                       }
                       BluetoothAdapter.STATE_ON -> {
                           // Bluetooth enabled, reinitialize
                           initBluetooth()
                       }
                   }
               }
           }
       }
   }
   ```

2. Register/unregister in lifecycle:
   ```kotlin
   override fun onResume() {
       super.onResume()
       val filter = IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED)
       registerReceiver(bluetoothStateReceiver, filter)
   }

   override fun onPause() {
       super.onPause()
       unregisterReceiver(bluetoothStateReceiver)
   }
   ```

**Verification:**
- Test disabling Bluetooth during scan, verify graceful stop
- Test enabling Bluetooth, verify reinitialization
- Test disabling during fuzz, verify cleanup

---

### Phase 3: Architecture Refactoring (Priority 3)

**Note:** This is a larger refactoring. See `ARCHITECTURE_REFACTOR_PLAN.md` for detailed steps.

**Summary:**
1. Extract `BleDevice` to separate file
2. Extract `DeviceAdapter` to separate file
3. Create `PermissionManager`
4. Create `BleScanManager`
5. Create `BleAdvertiseManager`
6. Create `FuzzScheduler`
7. Create `BleController` coordinator
8. Refactor `MainActivity` to use `BleController`

**Verification:**
- All existing functionality works
- Unit tests pass
- No performance regression

---

### Phase 4: Additional Improvements (Priority 4)

#### 4.1 Add Payload Size Validation

**File:** `app/src/main/java/com/axon/blecontroller/MainActivity.kt` (or `BleAdvertiseManager.kt` if refactored)

**Changes:**
```kotlin
private fun validatePayloadSize(serviceData: ByteArray): Boolean {
    // BLE advertising data limit: 31 bytes total
    // Service UUID overhead: ~2-4 bytes
    // Service data: serviceData.size bytes
    // Other overhead: ~2-3 bytes
    val totalSize = 4 + serviceData.size + 3
    return totalSize <= 31
}

private fun startAdvertisingInternal() {
    if (!validatePayloadSize(currentServiceData)) {
        Toast.makeText(this, ">> ERROR: PAYLOAD TOO LARGE", Toast.LENGTH_LONG).show()
        return
    }
    // ... rest of implementation
}
```

#### 4.2 Add Error Recovery/Retry Logic

**File:** `app/src/main/java/com/axon/blecontroller/MainActivity.kt`

**Changes:**
- Add retry counters and exponential backoff for scan/advertise failures
- Stop operations after max retries

#### 4.3 Add Fuzz Stop Conditions

**File:** `app/src/main/java/com/axon/blecontroller/MainActivity.kt`

**Changes:**
- Add max iterations option
- Add timeout option
- Stop on consecutive failures

---

## Testing Checklist

### Phase 1 Testing
- [ ] Android backup disabled (check manifest)
- [ ] No sensitive data in release logs (build release APK, check logcat)
- [ ] Bluetooth enable prompt appears when Bluetooth disabled
- [ ] App handles Bluetooth enable denial gracefully
- [ ] Fuzz loop doesn't leak memory (use memory profiler)
- [ ] ScanFilter works (verify only 0xFE6C devices appear)
- [ ] ScanFilter fallback works on unsupported devices

### Phase 2 Testing
- [ ] Sequential fuzz mode works (0x0000 → 0xFFFF)
- [ ] Random fuzz mode works (random values, no immediate duplicates)
- [ ] Fuzz mode toggle works in UI
- [ ] Adapter state monitoring works (disable/enable Bluetooth during operation)

### Phase 3 Testing
- [ ] All existing functionality works after refactor
- [ ] Unit tests pass
- [ ] No performance regression

### Phase 4 Testing
- [ ] Payload size validation prevents oversized payloads
- [ ] Error recovery works (retry on transient failures)
- [ ] Fuzz stop conditions work (max iterations, timeout)

---

## File Change Summary

### Files to Modify
1. `app/src/main/AndroidManifest.xml` - Disable backup
2. `app/src/main/java/com/axon/blecontroller/MainActivity.kt` - All fixes and features

### Files to Create (Phase 3)
1. `app/src/main/java/com/axon/blecontroller/data/BleDevice.kt`
2. `app/src/main/java/com/axon/blecontroller/ui/DeviceAdapter.kt`
3. `app/src/main/java/com/axon/blecontroller/permissions/PermissionManager.kt`
4. `app/src/main/java/com/axon/blecontroller/ble/BleController.kt`
5. `app/src/main/java/com/axon/blecontroller/ble/BleScanManager.kt`
6. `app/src/main/java/com/axon/blecontroller/ble/BleAdvertiseManager.kt`
7. `app/src/main/java/com/axon/blecontroller/ble/FuzzScheduler.kt`
8. `app/src/main/java/com/axon/blecontroller/ble/BleState.kt`

---

**End of Implementation Plan**
