// Copyright CitraVR Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.updatechecker

import org.citra.citra_emu.features.updatechecker.UpdateChecker.Version
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class VersionTest {

    private fun version(tag: String): Version =
        Version.parse(tag) ?: throw AssertionError("expected $tag to parse")

    private fun assertNewer(newer: String, older: String) {
        assertTrue("$newer should outrank $older", version(newer) > version(older))
        assertTrue("$older should not outrank $newer", version(older) < version(newer))
    }

    @Test
    fun stableTagsParse() {
        version("v0.5.2")
        version("0.5.2") // leading 'v' optional
        version("v1.0")
    }

    @Test
    fun prereleaseTagsParse() {
        assertTrue(version("v0.6.0-beta.1").isPreRelease)
        assertTrue(!version("v0.6.0").isPreRelease)
    }

    @Test
    fun playtestStyleTagsDoNotParse() {
        assertNull(Version.parse("playtest02.17.2024"))
        assertNull(Version.parse("test-hzos-openxr-loader-001"))
        assertNull(Version.parse("playtest-2026-07-18"))
        assertNull(Version.parse(""))
        assertNull(Version.parse("v"))
        assertNull(Version.parse("v0.6.0-beta")) // prerelease series must be numbered
    }

    @Test
    fun stableOrdering() {
        assertNewer("v0.5.2", "v0.5.1")
        assertNewer("v0.10.0", "v0.9.9") // numeric, not lexicographic
        assertNewer("v1.0.0", "v0.99.99")
        assertNewer("v0.5.0.1", "v0.5") // shorter version padded with zeros
        assertTrue(version("v0.5.0").compareTo(version("v0.5")) == 0)
    }

    @Test
    fun stableOutranksItsPrereleases() {
        assertNewer("v0.6.0", "v0.6.0-rc.9")
        assertNewer("v0.6.0-beta.1", "v0.5.2")
    }

    @Test
    fun prereleaseSeriesOrdering() {
        assertNewer("v0.6.0-beta.2", "v0.6.0-beta.1")
        assertNewer("v0.6.0-beta.10", "v0.6.0-beta.9")
        assertNewer("v0.6.1-beta.1", "v0.6.0-beta.2")
    }
}
