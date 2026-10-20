// Copyright CitraVR Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.vr.utils

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class VrQuickStateChordTest {
    private var enabled = true
    private var saves = 0
    private var loads = 0
    private lateinit var chord: VrQuickStateChord

    private val a = KeyEvent.KEYCODE_BUTTON_A
    private val b = KeyEvent.KEYCODE_BUTTON_B
    private val zl = KeyEvent.KEYCODE_BUTTON_L2
    private val zr = KeyEvent.KEYCODE_BUTTON_R2

    @Before
    fun setUp() {
        enabled = true
        saves = 0
        loads = 0
        chord = VrQuickStateChord({ enabled }, { saves++ }, { loads++ })
    }

    private fun holdTriggers() {
        chord.onButton(zl, true)
        chord.onButton(zr, true)
    }

    @Test
    fun triggersAreNeverConsumed() {
        assertFalse(chord.onButton(zl, true))
        assertFalse(chord.onButton(zr, true))
        assertFalse(chord.onButton(zl, false))
        assertFalse(chord.onButton(zr, false))
    }

    @Test
    fun buttonsPassThroughWithoutChord() {
        assertFalse(chord.onButton(a, true))
        assertFalse(chord.onButton(a, false))
        assertFalse(chord.onButton(b, true))
        assertFalse(chord.onButton(b, false))
        assertEquals(0, saves)
        assertEquals(0, loads)
    }

    @Test
    fun oneTriggerIsNotAChord() {
        chord.onButton(zl, true)
        assertFalse(chord.onButton(a, true))
        assertEquals(0, saves)
    }

    @Test
    fun aSavesAndBLoads() {
        holdTriggers()
        assertTrue(chord.onButton(a, true))
        assertEquals(1, saves)
        assertEquals(0, loads)
        assertTrue(chord.onButton(a, false))
        assertTrue(chord.onButton(b, true))
        assertEquals(1, saves)
        assertEquals(1, loads)
        assertTrue(chord.onButton(b, false))
    }

    @Test
    fun heldButtonTriggersOnce() {
        holdTriggers()
        // The VR thread repeats the press event on every frame while held.
        repeat(5) { assertTrue(chord.onButton(a, true)) }
        assertEquals(1, saves)
        assertTrue(chord.onButton(a, false))
        // A new physical press is a new chord.
        assertTrue(chord.onButton(a, true))
        assertEquals(2, saves)
    }

    @Test
    fun releaseAfterTriggersLetGoIsStillConsumed() {
        holdTriggers()
        assertTrue(chord.onButton(a, true))
        chord.onButton(zl, false)
        chord.onButton(zr, false)
        assertTrue(chord.onButton(a, false))
        // And afterwards A behaves normally again.
        assertFalse(chord.onButton(a, true))
        assertFalse(chord.onButton(a, false))
        assertEquals(1, saves)
    }

    @Test
    fun buttonHeldBeforeTriggersIsNotAChord() {
        assertFalse(chord.onButton(a, true))
        holdTriggers()
        assertFalse(chord.onButton(a, true))
        assertFalse(chord.onButton(a, false))
        assertEquals(0, saves)
    }

    @Test
    fun disabledChordConsumesNothing() {
        enabled = false
        holdTriggers()
        assertFalse(chord.onButton(a, true))
        assertFalse(chord.onButton(a, false))
        assertFalse(chord.onButton(b, true))
        assertFalse(chord.onButton(b, false))
        assertEquals(0, saves)
        assertEquals(0, loads)
    }

    @Test
    fun otherButtonsAreIgnored() {
        holdTriggers()
        assertFalse(chord.onButton(KeyEvent.KEYCODE_BUTTON_X, true))
        assertFalse(chord.onButton(KeyEvent.KEYCODE_BUTTON_Y, true))
        assertFalse(chord.onButton(KeyEvent.KEYCODE_BUTTON_L1, true))
        assertEquals(0, saves)
        assertEquals(0, loads)
    }
}
