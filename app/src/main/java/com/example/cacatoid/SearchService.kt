package com.example.cacatoid

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Foreground service that owns the native key search. Running the search here,
 * rather than in an Activity-scoped ViewModel, keeps the process alive and at
 * foreground priority while the app is backgrounded or the screen is off. A
 * partial wake lock stops the CPU from sleeping mid-search, and the ongoing
 * notification both satisfies the foreground-service requirement and gives the
 * user a way to stop the search without reopening the app.
 */
class SearchService : Service(), NativeBridge.SearchListener {

    private var wakeLock: PowerManager.WakeLock? = null
    private var puzzle: Int = 71
    private var lastNotifyAt = 0L

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopSearch()
            return START_NOT_STICKY
        }
        startSearch(intent?.getIntExtra(EXTRA_PUZZLE, puzzle) ?: puzzle)
        // Redeliver the original intent if the system restarts us, so the search
        // resumes on the same puzzle rather than a default.
        return START_REDELIVER_INTENT
    }

    private fun startSearch(puzzle: Int) {
        if (SearchState.running.value == true) return
        this.puzzle = puzzle

        SearchState.reset()
        SearchState.setRunning(true)

        createChannel()
        startForegroundCompat(buildNotification(getString(R.string.notif_starting)))
        acquireWakeLock()

        NativeBridge.nativeStart(puzzle, this)
    }

    private fun stopSearch() {
        NativeBridge.nativeStop()
        SearchState.setRunning(false)
        releaseWakeLock()
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    override fun onDestroy() {
        // Safety net: if the system tears the service down without ACTION_STOP,
        // make sure the native threads and wake lock don't leak.
        NativeBridge.nativeStop()
        releaseWakeLock()
        SearchState.setRunning(false)
        super.onDestroy()
    }

    // --- NativeBridge.SearchListener (invoked from native threads) ---

    override fun onStats(currentKeyHex: String, keysPerSec: Long, totalChecked: Long) {
        SearchState.setStats(SearchState.Stats(currentKeyHex, keysPerSec, totalChecked))
        // The native monitor fires roughly twice a second; rate-limit the
        // notification rebuild so we don't thrash the status bar.
        val now = System.currentTimeMillis()
        if (now - lastNotifyAt >= NOTIFY_INTERVAL_MS) {
            lastNotifyAt = now
            notify(
                buildNotification(
                    getString(
                        R.string.notif_running,
                        puzzle,
                        "%,d".format(keysPerSec),
                        "%,d".format(totalChecked),
                    )
                )
            )
        }
    }

    override fun onFound(privKeyHex: String, wif: String, address: String, puzzle: Int) {
        val result = SearchState.Found(puzzle, privKeyHex, wif, address)
        persist(result)
        SearchState.setFound(result)
        SearchState.setRunning(false)
        releaseWakeLock()
        // Leave a final, dismissible notification behind so the user sees the hit
        // even if the app was in the background when it landed.
        notify(buildNotification(getString(R.string.notif_found, puzzle), ongoing = false))
        stopForeground(STOP_FOREGROUND_DETACH)
        stopSelf()
    }

    /** Writes the hit to internal storage so it survives an app restart. */
    private fun persist(found: SearchState.Found) {
        runCatching {
            val ts = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date())
            val file = File(filesDir, "found_keys.txt")
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

    // --- Notification / wake lock plumbing ---

    private fun startForegroundCompat(notification: android.app.Notification) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            startForeground(
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE,
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun notify(notification: android.app.Notification) {
        val nm = getSystemService(NotificationManager::class.java)
        nm.notify(NOTIFICATION_ID, notification)
    }

    private fun createChannel() {
        val nm = getSystemService(NotificationManager::class.java)
        if (nm.getNotificationChannel(CHANNEL_ID) == null) {
            nm.createNotificationChannel(
                NotificationChannel(
                    CHANNEL_ID,
                    getString(R.string.notif_channel_name),
                    NotificationManager.IMPORTANCE_LOW,
                ).apply { setShowBadge(false) }
            )
        }
    }

    private fun buildNotification(text: String, ongoing: Boolean = true): android.app.Notification {
        val openIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE,
        )
        val builder = NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.app_name))
            .setContentText(text)
            .setSmallIcon(R.drawable.ic_bolt)
            .setContentIntent(openIntent)
            .setOngoing(ongoing)
            .setSilent(true)
        if (ongoing) {
            val stopIntent = PendingIntent.getService(
                this,
                1,
                Intent(this, SearchService::class.java).setAction(ACTION_STOP),
                PendingIntent.FLAG_IMMUTABLE,
            )
            builder.addAction(R.drawable.ic_stop, getString(R.string.stop), stopIntent)
        }
        return builder.build()
    }

    private fun acquireWakeLock() {
        if (wakeLock?.isHeld == true) return
        val pm = getSystemService(PowerManager::class.java)
        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, WAKE_LOCK_TAG).apply {
            setReferenceCounted(false)
            acquire()
        }
    }

    private fun releaseWakeLock() {
        wakeLock?.let { if (it.isHeld) it.release() }
        wakeLock = null
    }

    companion object {
        const val EXTRA_PUZZLE = "puzzle"
        const val ACTION_STOP = "com.example.cacatoid.action.STOP"

        private const val CHANNEL_ID = "search"
        private const val NOTIFICATION_ID = 1
        private const val NOTIFY_INTERVAL_MS = 1000L
        private const val WAKE_LOCK_TAG = "cacatoid:search"
    }
}
