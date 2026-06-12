package com.example.cacatoid

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData

/**
 * Process-wide search state, deliberately decoupled from any Activity or
 * ViewModel. [SearchService] owns the running search and writes here; the UI
 * observes. Because this outlives the Activity, a search keeps publishing
 * updates even after the UI is destroyed (e.g. the user swipes the app away
 * while it runs in the background).
 */
object SearchState {

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

    /** Last puzzle the user chose; retained so the dropdown restores correctly. */
    var selectedPuzzle: Int = 71

    fun setRunning(value: Boolean) = _running.postValue(value)

    fun setStats(value: Stats) = _stats.postValue(value)

    fun setFound(value: Found?) = _found.postValue(value)

    /** Clears stats and any prior result at the start of a new search. */
    fun reset() {
        _stats.postValue(Stats())
        _found.postValue(null)
    }
}
