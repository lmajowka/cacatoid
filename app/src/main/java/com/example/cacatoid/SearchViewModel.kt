package com.example.cacatoid

import android.app.Application
import android.content.Intent
import androidx.core.content.ContextCompat
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData

/**
 * Thin UI-facing facade over the search. The search itself runs in
 * [SearchService] (a foreground service) so it survives the Activity being
 * backgrounded or destroyed; this ViewModel just relays start/stop commands and
 * re-exposes the process-wide [SearchState] for the UI to observe.
 */
class SearchViewModel(app: Application) : AndroidViewModel(app) {

    val running: LiveData<Boolean> = SearchState.running
    val stats: LiveData<SearchState.Stats> = SearchState.stats
    val found: LiveData<SearchState.Found?> = SearchState.found

    var selectedPuzzle: Int
        get() = SearchState.selectedPuzzle
        set(value) { SearchState.selectedPuzzle = value }

    fun start() {
        if (running.value == true) return
        val intent = Intent(getApplication(), SearchService::class.java)
            .putExtra(SearchService.EXTRA_PUZZLE, selectedPuzzle)
        ContextCompat.startForegroundService(getApplication(), intent)
    }

    fun stop() {
        if (running.value == false) return
        val intent = Intent(getApplication(), SearchService::class.java)
            .setAction(SearchService.ACTION_STOP)
        getApplication<Application>().startService(intent)
    }
}
