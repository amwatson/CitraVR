package org.citra.citra_emu.vr.utils

object VrMessageQueue {
        // Keep consistent with MessageQueue.h
        enum class MessageType(val jniVal: Int) {
                SHOW_KEYBOARD(0),
                SHOW_ERROR_MESSAGE(1),
                EXIT_NEEDED(2),
                CHANGE_LOWER_MENU(3),
                CHANGE_LOCK_HORIZONTAL_AXIS(4),
                RESET_PANEL_POSITIONS(5)
            }
        @JvmStatic
        fun post(messageType: MessageType, payload: Long) {
                nativePost(messageType.jniVal, payload)
            }
        private external fun nativePost(messageType: Int, payload: Long)
    }