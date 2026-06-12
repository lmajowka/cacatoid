package com.example.cacatoid

/**
 * JNI bridge to the native Bitcoin puzzle searcher.
 *
 * All cryptography and key iteration runs in C++ (see app/src/main/cpp). The
 * native side spawns one worker thread per CPU core plus a monitor thread that
 * invokes [SearchListener.onStats] roughly every 500 ms. A match triggers
 * [SearchListener.onFound] immediately from the worker that found it.
 */
object NativeBridge {

    init {
        System.loadLibrary("cacatoid")
    }

    /** Callbacks delivered from native threads. Implementations must marshal to
     *  the main thread themselves before touching UI. */
    interface SearchListener {
        /** @param currentKeyHex 64-char hex of a key currently under test
         *  @param keysPerSec    aggregate throughput across all threads
         *  @param totalChecked  total keys checked since the search started */
        fun onStats(currentKeyHex: String, keysPerSec: Long, totalChecked: Long)

        /** Fired once when a target address is matched. */
        fun onFound(privKeyHex: String, wif: String, address: String, puzzle: Int)
    }

    /** Starts (or restarts) the search for the given puzzle (71, 72 or 73). */
    external fun nativeStart(puzzle: Int, listener: SearchListener)

    /** Stops the search and joins all native threads. Safe to call when idle. */
    external fun nativeStop()
}
