package com.gbdesk.android.native

import android.view.Surface

data class SaveSlotInfo(
    val slot: Int,
    val exists: Boolean,
    val timestampMs: Long
)

object CoreBridge {
    init {
        System.loadLibrary("gbdesk_jni")
    }

    const val KEY_A = 1 shl 0
    const val KEY_B = 1 shl 1
    const val KEY_SELECT = 1 shl 2
    const val KEY_START = 1 shl 3
    const val KEY_RIGHT = 1 shl 4
    const val KEY_LEFT = 1 shl 5
    const val KEY_UP = 1 shl 6
    const val KEY_DOWN = 1 shl 7
    const val KEY_R = 1 shl 8
    const val KEY_L = 1 shl 9

    external fun nativeLoadRomFd(fd: Int, baseName: String, saveDir: String): Boolean
    external fun nativeSetSurface(surface: Surface?)
    external fun nativeSetPaused(paused: Boolean)
    external fun nativeSetKeys(keys: Int)
    external fun nativeReset()
    external fun nativeStop()
    external fun nativeGetStateTimestamp(slot: Int): Long
    external fun nativeLoadStateSlot(slot: Int): Boolean
    external fun nativeSaveStateSlot(slot: Int): Boolean
    external fun nativeLoadStateFd(fd: Int): Boolean

    fun getSaveSlots(): List<SaveSlotInfo> {
        return (1..9).map { slot ->
            val ts = nativeGetStateTimestamp(slot)
            SaveSlotInfo(slot = slot, exists = ts >= 0, timestampMs = ts)
        }
    }
}
