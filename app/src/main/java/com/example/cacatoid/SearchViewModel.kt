package com.example.cacatoid

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.LiveData
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Owns the search lifecycle and exposes observable state to the UI. Survives
 * configuration changes so a running search keeps reporting after rotation.
 */
class SearchViewModel(app: Application) : AndroidViewModel(app), NativeBridge.SearchListener {

    data class Stats(
        val currentKeyHex: String = "",
        val keysPerSec: Long = 0,
        val totalChecked: Long = 0,
    )

    data class Found(
        val puzzle: Int,
        val privKeyHex: String,
        val wif: String,
        val address: String,
    )

    private val _running = MutableLiveData(false)
    val running: LiveData<Boolean> = _running

    private val _stats = MutableLiveData(Stats())
    val stats: LiveData<Stats> = _stats

    /** Non-null once a key is found; the UI observes this to surface the result. */
    private val _found = MutableLiveData<Found?>(null)
    val found: LiveData<Found?> = _found

    var selectedPuzzle: Int = 71

    fun start() {
        if (_running.value == true) return
        _found.postValue(null)
        _stats.postValue(Stats())
        _running.value = true
        NativeBridge.nativeStart(selectedPuzzle, this)
    }

    fun stop() {
        if (_running.value == false) return
        NativeBridge.nativeStop()
        _running.value = false
    }

    // --- NativeBridge.SearchListener (invoked from native threads) ---

    override fun onStats(currentKeyHex: String, keysPerSec: Long, totalChecked: Long) {
        _stats.postValue(Stats(currentKeyHex, keysPerSec, totalChecked))
    }

    override fun onFound(privKeyHex: String, wif: String, address: String, puzzle: Int) {
        val result = Found(puzzle, privKeyHex, wif, address)
        persist(result)
        _found.postValue(result)
        _running.postValue(false)
    }

    /** Writes the hit to internal storage so it survives an app restart. */
    private fun persist(found: Found) {
        runCatching {
            val ts = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date())
            val file = File(getApplication<Application>().filesDir, "found_keys.txt")
            file.appendText(
                buildString {
                    append("[$ts] puzzle ${found.puzzle}\n")
                    append("  privkey_hex: ${found.privKeyHex}\n")
                    append("  wif:         ${found.wif}\n")
                    append("  address:     ${found.address}\n\n")
                }
            )
        }
    }

    override fun onCleared() {
        super.onCleared()
        NativeBridge.nativeStop()
    }
}
