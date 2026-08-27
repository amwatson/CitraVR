package org.citra.citra_emu.vr.ui

import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.ImageButton
import android.widget.TextView
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.button.MaterialButtonToggleGroup
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.R
import org.citra.citra_emu.features.settings.model.AbstractSetting
import org.citra.citra_emu.features.settings.model.Settings
import org.citra.citra_emu.features.settings.model.view.RunnableSetting
import org.citra.citra_emu.features.settings.model.view.SingleChoiceSetting
import org.citra.citra_emu.features.settings.model.view.SliderSetting
import org.citra.citra_emu.features.settings.model.view.SettingsItem
import org.citra.citra_emu.features.settings.model.view.SwitchSetting
import org.citra.citra_emu.features.settings.ui.SettingsActivityView
import org.citra.citra_emu.features.settings.ui.SettingsAdapter
import org.citra.citra_emu.features.settings.ui.SettingsFragmentPresenter
import org.citra.citra_emu.features.settings.ui.SettingsFragmentView
import org.citra.citra_emu.features.settings.utils.PerGameSettings
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
class VrSettingsMenu(panelRoot: View, private val gameTitle: String?) :
    SettingsFragmentView,
    SettingsActivityView {

    override val settings = Settings()

    var adapter: SettingsAdapter? = null
        private set

    private var presenter: SettingsFragmentPresenter? = null
    private val menuStack = ArrayDeque<String>()

    private val listView: RecyclerView = panelRoot.findViewById(R.id.settings_list)
    private val titleView: TextView = panelRoot.findViewById(R.id.settings_title)
    private val backButton: ImageButton = panelRoot.findViewById(R.id.settings_back)
    private val perGameBanner: View = panelRoot.findViewById(R.id.per_game_settings_banner)
    private val perGameStatus: TextView = panelRoot.findViewById(R.id.per_game_settings_status)
    private val scopeToggle: MaterialButtonToggleGroup =
        panelRoot.findViewById(R.id.settings_scope_toggle)

    private val handler = Handler(Looper.getMainLooper())
    private var isDirty = false
    private val applyRunnable = Runnable { saveAndApply() }
    private var activeTitleId: String? = null
    private var perGameSnapshot: PerGameSettings.Snapshot? = null
    private var baselineValues: Map<String, String> = emptyMap()
    private var runningValues: Map<String, String> = emptyMap()
    private var scope = Scope.PER_GAME

    init {
        backButton.setOnClickListener { goBack() }
        scopeToggle.addOnButtonCheckedListener { _, checkedId, isChecked ->
            if (!isChecked) return@addOnButtonCheckedListener
            when (checkedId) {
                R.id.settings_scope_global -> switchScope(Scope.GLOBAL)
                R.id.settings_scope_per_game -> switchScope(Scope.PER_GAME)
            }
        }
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
        // Reload from disk on every presentation so deleting per_game.ini.vr or
        // editing it outside the app is reflected without retaining stale state.
        settings.loadSettings(this)
        if (scope == Scope.PER_GAME) {
            loadPerGameSettings()
        } else {
            updateBanner()
        }
        updateScopeToggle()
        if (menuStack.isEmpty()) {
            showSettingsFragment(SettingsFile.FILE_NAME_CONFIG, false, "")
        } else {
            loadMenu(menuStack.last())
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
        val resetSettings = if (scope == Scope.PER_GAME) ::showResetPerGameDialog else null
        presenter = SettingsFragmentPresenter(this, resetSettings).also {
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

        if (scope == Scope.GLOBAL) {
            settings.saveSettings(this)
            NativeLibrary.reloadSettings()
            return
        }

        val currentValues = PerGameSettings.readCurrentValues()
        val titleId = activeTitleId
        val snapshot = perGameSnapshot
        if (titleId != null && snapshot != null) {
            val changedValues = currentValues.filter { (key, value) ->
                baselineValues[key] != value
            }
            if (changedValues.isNotEmpty()) {
                if (PerGameSettings.writeUserValues(titleId, changedValues)) {
                    perGameSnapshot = snapshot.copy(hasTitleSettings = true)
                    baselineValues = currentValues
                } else {
                    // Leave the menu dirty so hiding it retries instead of silently
                    // dropping a per-game selection.
                    isDirty = true
                    return
                }
            }
        }

        NativeLibrary.reloadSettings()
        updateBanner()
        presenter?.loadSettingsList()
    }

    private fun loadPerGameSettings() {
        val runningTitleId = NativeLibrary.getRunningTitleId()
        if (runningTitleId == 0L) {
            activeTitleId = null
            perGameSnapshot = null
            baselineValues = emptyMap()
            runningValues = emptyMap()
            perGameBanner.visibility = View.GONE
            return
        }

        val titleId = PerGameSettings.formatTitleId(runningTitleId)
        val snapshot = PerGameSettings.load(titleId, settings)
        val resolvedValues = PerGameSettings.readCurrentValues()
        if (activeTitleId != titleId || runningValues.isEmpty()) {
            runningValues = resolvedValues
        }
        activeTitleId = titleId
        perGameSnapshot = snapshot
        baselineValues = resolvedValues
        updateBanner()
    }

    private fun switchScope(newScope: Scope) {
        if (scope == newScope) {
            updateScopeToggle()
            return
        }

        handler.removeCallbacks(applyRunnable)
        saveAndApply()
        if (isDirty) {
            updateScopeToggle()
            return
        }

        scope = newScope
        settings.loadSettings(this)
        if (scope == Scope.PER_GAME) {
            loadPerGameSettings()
        } else {
            updateBanner()
        }
        updateScopeToggle()
        menuStack.lastOrNull()?.let(::loadMenu)
    }

    private fun updateScopeToggle() {
        scopeToggle.check(
            if (scope == Scope.GLOBAL) {
                R.id.settings_scope_global
            } else {
                R.id.settings_scope_per_game
            }
        )
    }

    private fun showResetPerGameDialog() {
        if (scope != Scope.PER_GAME || activeTitleId == null) return
        adapter?.showConfirmationDialog(
            R.string.reset_per_game_settings,
            R.string.reset_per_game_settings_description
        ) {
            resetPerGameSettings()
        }
    }

    private fun resetPerGameSettings() {
        val titleId = activeTitleId ?: return
        handler.removeCallbacks(applyRunnable)
        isDirty = false
        if (!PerGameSettings.clearUserValues(titleId)) {
            adapter?.showMessageDialog(
                R.string.reset_per_game_settings,
                R.string.reset_per_game_settings_failed
            )
            return
        }

        settings.loadSettings(this)
        loadPerGameSettings()
        NativeLibrary.reloadSettings()
        presenter?.loadSettingsList()
        // Settings are process-wide objects, so old and new rows see the same
        // updated value during DiffUtil comparison. Force their visible values
        // to be rebound after the reset.
        adapter?.notifyDataSetChanged()
    }

    private fun updateBanner() {
        val titleId = activeTitleId ?: run {
            perGameBanner.visibility = View.GONE
            return
        }
        val snapshot = perGameSnapshot ?: return
        perGameBanner.visibility = View.VISIBLE
        if (scope == Scope.GLOBAL) {
            perGameStatus.setText(R.string.global_settings_status)
            return
        }
        val gameLabel = gameTitle?.takeIf(String::isNotBlank) ?: titleId
        val status = if (snapshot.hasTitleSettings) {
            listView.context.getString(R.string.per_game_settings_loaded, gameLabel)
        } else {
            listView.context.getString(R.string.per_game_settings_not_loaded, gameLabel)
        }
        val currentValues = PerGameSettings.readCurrentValues()
        val hasPendingRestart = PerGameSettings.definitions.any { definition ->
            definition.restartRequired &&
                runningValues[definition.key] !=
                currentValues[definition.key]
        }
        perGameStatus.text = if (hasPendingRestart) {
            "$status\n${listView.context.getString(R.string.per_game_restart_pending)}"
        } else {
            status
        }
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
        if (scope == Scope.PER_GAME) {
            val snapshot = perGameSnapshot
            settingsList.forEach { item ->
                val definition = PerGameSettings.definitionFor(item.setting?.key)
                val isPerGameReset =
                    item is RunnableSetting && item.nameId == R.string.reset_to_default
                if (definition != null && snapshot != null) {
                    decoratePerGameSetting(item, snapshot)
                } else if (isPerGameReset) {
                    item.isReadOnly = activeTitleId == null
                } else if (item.setting != null || item is RunnableSetting) {
                    // This compatibility file currently supports only the declared
                    // per-title keys; do not let other settings silently save globally.
                    item.isReadOnly = true
                }
            }
        }
        adapter?.setSettingsList(settingsList)
    }

    private fun decoratePerGameSetting(
        item: SettingsItem,
        snapshot: PerGameSettings.Snapshot
    ) {
        val definition = PerGameSettings.definitionFor(item.setting?.key) ?: return
        item.allowRuntimeStaging = true

        val statusLines = mutableListOf<String>()
        if (snapshot.isBaseOverriddenByGlobal(definition.key)) {
            val globalValue = displayValue(item, snapshot.globalCustomValues.getValue(definition.key))
            val baseValue = displayValue(item, snapshot.baseValues.getValue(definition.key))
            item.isOverriddenByGlobal = true
            item.configuredPerGameChoice = snapshot.baseValues[definition.key]?.toIntOrNull()
            statusLines += listView.context.getString(
                R.string.per_game_overridden_by_global,
                globalValue,
                baseValue
            )
        }
        if (definition.restartRequired) {
            statusLines += listView.context.getString(R.string.per_game_restart_required)
        }
        item.perGameStatusText = statusLines.takeIf { it.isNotEmpty() }?.joinToString("\n")
    }

    private fun displayValue(item: SettingsItem, value: String): String {
        return when (item) {
            is SingleChoiceSetting -> {
                val intValue = value.toIntOrNull()
                val values = listView.context.resources.getIntArray(item.valuesId)
                val names = listView.context.resources.getStringArray(item.choicesId)
                val index = intValue?.let(values::indexOf) ?: -1
                index.takeIf { it >= 0 }?.let { names[it] } ?: value
            }

            is SwitchSetting -> if (value.toBoolean()) {
                listView.context.getString(R.string.setting_value_enabled)
            } else {
                listView.context.getString(R.string.setting_value_disabled)
            }

            is SliderSetting -> "$value${item.units}"
            else -> value
        }
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

    private enum class Scope {
        GLOBAL,
        PER_GAME
    }
}
