# Cacatoid

An Android app that searches for the private key to unsolved [Bitcoin Puzzle](https://privatekeys.pw/puzzles/bitcoin-puzzle-tx) addresses, running a multi-threaded native key search directly on your phone.

> ⚠️ **For fun and learning.** The puzzles Cacatoid targets (71, 72, 73) have key spaces of roughly 2⁷⁰–2⁷². A modern phone checks a few hundred thousand to a few million keys per second, so the odds of an actual hit are astronomically small. Treat this as a demonstration of secp256k1 cryptography and Android background processing, not a get-rich plan.

## What it does

The [Bitcoin puzzle transaction](https://privatekeys.pw/puzzles/bitcoin-puzzle-tx) is a well-known challenge where puzzle *n* has a private key somewhere in the range `[2^(n-1), 2^n - 1]`, and the matching address is public. Cacatoid picks a target puzzle, derives the compressed P2PKH address for candidate keys, and compares it against the target.

- **Puzzle 20** — already solved; included as a built-in test because its tiny range is found almost instantly, exercising the whole pipeline end to end.
- **Puzzles 71, 72, 73** — currently unsolved targets.

When a key matches, the app shows the private key (hex), WIF, and address, copies the key to the clipboard on request, and appends the hit to `found_keys.txt` in internal storage so it survives a restart.

## How it works

```
MainActivity ──▶ SearchViewModel ──▶ SearchService (foreground) ──▶ NativeBridge (JNI) ──▶ C++ searcher
```

- **Native search (`app/src/main/cpp`)** — all cryptography and key iteration runs in C++: a self-contained secp256k1 point multiply, SHA-256, RIPEMD-160, and Base58Check. The searcher spawns one worker thread per CPU core, splits the puzzle's range into sub-ranges, and starts each worker at a random offset within its slice. A monitor thread reports throughput to the JVM about twice a second; a match fires a callback immediately.
- **Foreground service (`SearchService`)** — owns the search so it keeps running at foreground priority while the app is backgrounded or the screen is off. It holds a partial wake lock to stop the CPU from sleeping mid-search and shows an ongoing notification with live stats and a stop action.
- **UI (`MainActivity` + `SearchViewModel`)** — pick a puzzle, start/stop, and watch the current key, keys/sec, and total-checked counters update live.

128-bit range math uses `unsigned __int128`, which clang only supports on 64-bit targets, so the build is restricted to the `arm64-v8a` and `x86_64` ABIs.

## Building

Requires Android Studio with the NDK and CMake installed.

```bash
./gradlew assembleDebug      # build a debug APK
./gradlew installDebug       # build and install on a connected device
```

- **minSdk** 28 · **targetSdk** 36
- Language: Kotlin (app) + C++ (search core), built with CMake via the Android Gradle Plugin

## Project layout

| Path | Purpose |
| --- | --- |
| `app/src/main/java/.../MainActivity.kt` | UI and live stats |
| `app/src/main/java/.../SearchViewModel.kt` | UI state, start/stop |
| `app/src/main/java/.../SearchService.kt` | Foreground service, wake lock, notification, result persistence |
| `app/src/main/java/.../NativeBridge.kt` | JNI entry points and callbacks |
| `app/src/main/cpp/bitcoin_searcher.cpp` | Threaded search core |
| `app/src/main/cpp/secp256k1_utils.*`, `sha256.*`, `ripemd160.*`, `base58.*` | Crypto primitives |

## License

No license specified yet.
