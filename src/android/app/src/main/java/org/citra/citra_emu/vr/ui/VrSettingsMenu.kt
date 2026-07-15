package org.citra.citra_emu.vr.ui

import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.ImageButton
import android.widget.TextView
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.R
import org.citra.citra_emu.features.settings.model.AbstractSetting
import org.citra.citra_emu.features.settings.model.Settings
import org.citra.citra_emu.features.settings.model.view.SettingsItem
import org.citra.citra_emu.features.settings.ui.SettingsActivityView
import org.citra.citra_emu.features.settings.ui.SettingsAdapter
import org.citra.citra_emu.features.settings.ui.SettingsFragmentPresenter
import org.citra.citra_emu.features.settings.ui.SettingsFragmentView
import org.citra.citra_emu.features.settings.utils.SettingsFile
import org.citra.citra_emu.utils.Log
import org.citra.citra_emu.utils.SystemSaveGame
import org.citra.citra_emu.utils.TurboHelper

/**
 * Hosts the standard settings UI inside the VR ribbon's settings panel.
 *
 * This is a third implementation of the settings MVP view interfaces
 * (alongside SettingsActivity/SettingsFragment): the same
 * SettingsFragmentPresenter builds the item lists and the same
 * SettingsAdapter renders them; only the Activity/Fragment shell is replaced.
 * Submenu navigation uses a menu-tag back stack instead of fragment
 * transactions.
 *
 * Unlike the 2D settings screen, which persists and applies settings when the
 * activity closes, changes here are saved to the INI and applied to the
 * running core (NativeLibrary.reloadSettings) as they are made, so their
 * effect is visible in-game immediately. Settings the core only reads at boot
 * (and VR settings, which are read at VR init) still require a restart.
 */
class VrSettingsMenu(panelRoot: View) : SettingsFragmentView, SettingsActivityView {

    override val settings = Settings()

    var adapter: SettingsAdapter? = null
        private set

    private var presenter: SettingsFragmentPresenter? = null
    private val menuStack = ArrayDeque<String>()

    private val listView: RecyclerView = panelRoot.findViewById(R.id.settings_list)
    private val titleView: TextView = panelRoot.findViewById(R.id.settings_title)
    private val backButton: ImageButton = panelRoot.findViewById(R.id.settings_back)

    private val handler = Handler(Looper.getMainLooper())
    private var isDirty = false
    private val applyRunnable = Runnable { saveAndApply() }

    init {
        backButton.setOnClickListener { goBack() }
    }

    /** Called when the settings tab becomes visible. */
    fun onShow() {
        SystemSaveGame.load()
        if (adapter == null) {
            // The panel root carries a Material theme (set in XML), which the
            // adapter needs both for inflating list items and for the dialogs
            // it opens.
            adapter = SettingsAdapter(this, listView.context)
            listView.adapter = adapter
            listView.layoutManager = LinearLayoutManager(listView.context)
        }
        if (!settings.isLoaded) {
            settings.loadSettings(this)
        }
        if (menuStack.isEmpty()) {
            showSettingsFragment(SettingsFile.FILE_NAME_CONFIG, false, "")
        }
    }

    /** Called when the settings tab is hidden or the ribbon goes away. */
    fun onHide() {
        adapter?.closeDialog()
        handler.removeCallbacks(applyRunnable)
        saveAndApply()
        SystemSaveGame.save()
        TurboHelper.reloadTurbo(false)
    }

    fun goBack(): Boolean {
        if (adapter?.activeDialog?.isShowing == true) {
            adapter?.closeDialog()
            return true
        }
        if (menuStack.size > 1) {
            menuStack.removeLast()
            loadMenu(menuStack.last())
            return true
        }
        return false
    }

    private fun loadMenu(menuTag: String) {
        presenter = SettingsFragmentPresenter(this).also {
            it.onCreate(menuTag, "")
            it.onViewCreated(adapter!!)
        }
        backButton.visibility = if (menuStack.size > 1) View.VISIBLE else View.INVISIBLE
        listView.scrollToPosition(0)
    }

    private fun saveAndApply() {
        if (!isDirty) {
            return
        }
        isDirty = false
        settings.saveSettings(this)
        NativeLibrary.reloadSettings()
    }

    //// SettingsActivityView ////

    override fun showSettingsFragment(menuTag: String, addToStack: Boolean, gameId: String) {
        if (addToStack) {
            menuStack.addLast(menuTag)
        } else {
            menuStack.clear()
            menuStack.addLast(menuTag)
        }
        loadMenu(menuTag)
    }

    override fun onSettingsFileLoaded() {}

    override fun onSettingsFileNotFound() {}

    override fun setToolbarTitle(title: String) {
        titleView.text = title
    }

    override fun showToastMessage(message: String, isLong: Boolean) {
        // Toasts don't render on the VR virtual display; log instead.
        Log.info("[VrSettingsMenu] $message")
    }

    override fun finish() {}

    override fun onSettingChanged() {
        isDirty = true
        // Debounce so a burst of changes results in a single INI write and
        // core settings reload.
        handler.removeCallbacks(applyRunnable)
        handler.postDelayed(applyRunnable, APPLY_DELAY_MS)
    }

    //// SettingsFragmentView ////

    override fun showSettingsList(settingsList: ArrayList<SettingsItem>) {
        adapter?.setSettingsList(settingsList)
    }

    override fun loadSettingsList() {
        presenter?.loadSettingsList()
    }

    override val activityView: SettingsActivityView get() = this

    override fun loadSubMenu(menuKey: String) {
        showSettingsFragment(menuKey, true, "")
    }

    override fun putSetting(setting: AbstractSetting) {
        presenter?.putSetting(setting)
    }

    companion object {
        private const val APPLY_DELAY_MS = 500L
    }
}
