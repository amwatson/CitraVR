package org.citra.citra_emu.vr.ui

import android.content.Context
import android.text.InputFilter
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import org.citra.citra_emu.R
import org.citra.citra_emu.applets.SoftwareKeyboard
import org.citra.citra_emu.utils.Log
import java.util.Locale

class VrKeyboardView : LinearLayout {

    private enum class KeyboardType {
        None,
        Abc,
        Num
    }

    private var mEditText: EditText? = null
    private var mIsShifted = false
    private var mKeyboardTypeCur = KeyboardType.None
    private var mLayoutInflator : LayoutInflater? = null
    private var mConfig : SoftwareKeyboard.KeyboardConfig? = null

    constructor(context: Context) : super(context) {
        init(null, 0)
    }

    // Constructor for inflating the view from XML
    constructor(context: Context, attrs: AttributeSet?) : super(context, attrs) {
        init(attrs, 0)
    }

    private fun init(attrs: AttributeSet?, defStyle: Int) {
        mLayoutInflator = context.getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater


    }

    override fun onFinishInflate() {
        super.onFinishInflate()
        mEditText = findViewById(R.id.vrKeyboardText)
        showKeyboardType(KeyboardType.Abc)
    }

    // Call from UI thread
    fun setConfig(config: SoftwareKeyboard.KeyboardConfig) {
        val editText: EditText = findViewById(R.id.vrKeyboardText)
        assert(editText != null)
        editText!!.apply {
            setHint(config!!.hintText)
            setSingleLine(!config.multilineMode)
            setFilters(
                arrayOf(
                    SoftwareKeyboard.Filter(),
                    InputFilter.LengthFilter(config.maxTextLength)
                )
            )
            // Needed to show cursor onscreen.
            requestFocus()
            // Note: unlike 2D citra, don't need to worry about
            // window insets as this view is surfaced in its own separate window
        }
        setupResultButtons(config)
    }

    private fun setupResultButtons(config: SoftwareKeyboard.KeyboardConfig?) {
        // Configure the result buttons
        findViewById<View>(R.id.keyPositive).setOnTouchListener { _, event ->
            if (event.action == MotionEvent.ACTION_DOWN) {
                SoftwareKeyboard.onFinishVrKeyboardPositive(mEditText!!.text.toString(), mConfig)
            }
            false
        }
        findViewById<View>(R.id.keyNeutral).setOnTouchListener { _, event ->
            if (event.action == MotionEvent.ACTION_DOWN) {
                SoftwareKeyboard.onFinishVrKeyboardNeutral()
            }
            false
        }
        findViewById<View>(R.id.keyNegative).setOnTouchListener { _, event ->
            if (event.action == MotionEvent.ACTION_DOWN) {
                SoftwareKeyboard.onFinishVrKeyboardNegative()
            }
            false
        }
        when (config!!.buttonConfig) {
            SoftwareKeyboard.ButtonConfig.Triple -> {
                findViewById<View>(R.id.keyNeutral).visibility = View.VISIBLE
                findViewById<View>(R.id.keyNegative).visibility = View.VISIBLE
                findViewById<View>(R.id.keyPositive).visibility = View.VISIBLE
            }

            SoftwareKeyboard.ButtonConfig.Dual -> {
                findViewById<View>(R.id.keyNegative).visibility = View.VISIBLE
                findViewById<View>(R.id.keyPositive).visibility = View.VISIBLE
            }

            SoftwareKeyboard.ButtonConfig.Single -> findViewById<View>(R.id.keyPositive).visibility =
                View.VISIBLE

            SoftwareKeyboard.ButtonConfig.None -> {}
            else -> {
                Log.error("Unknown button config: " + config.buttonConfig)
                assert(false)
            }
        }
    }

    private fun showKeyboardType(keyboardType: KeyboardType) {
        if (mKeyboardTypeCur == keyboardType) {
            return
        }
        mKeyboardTypeCur = keyboardType
        val keyboard = findViewById<ViewGroup>(R.id.vr_keyboard_keyboard)
        keyboard.removeAllViews()
        when (keyboardType) {
            KeyboardType.Abc -> {
                mLayoutInflator!!.inflate(R.layout.vr_keyboard_abc, keyboard)
                addLetterKeyHandlersForViewGroup(keyboard, mIsShifted)
            }

            KeyboardType.Num -> {
                mLayoutInflator!!.inflate(R.layout.vr_keyboard_123, keyboard)
                addLetterKeyHandlersForViewGroup(keyboard, false)
            }

            else -> assert(false)
        }
        addModifierKeyHandlers()
    }

    private fun addModifierKeyHandlers() {
        findViewById<View>(R.id.keyShift).setOnTouchListener { _, event ->
            if (event.action == MotionEvent.ACTION_DOWN) {
                setKeyCase(!mIsShifted)
            }
            false
        }
        // Note: I prefer touch listeners over click listeners because they activate
        // on the press instead of the release and therefore feel more responsive.
        findViewById<View>(R.id.keyBackspace).setOnTouchListener { _, event ->
            if (event.action == MotionEvent.ACTION_DOWN) {
                val text = mEditText!!.text.toString()
                if (text.length > 0) {
                    // Delete character before cursor
                    val position = mEditText!!.selectionStart
                    if (position > 0) {
                        val newText = text.substring(0, position - 1) + text.substring(position)
                        mEditText!!.setText(newText)
                        mEditText!!.setSelection(position - 1)
                    }
                }
            }
            false
        }
        findViewById<View>(R.id.keySpace).setOnTouchListener { _, event ->
            if (event.action == MotionEvent.ACTION_DOWN) {
                val position = mEditText!!.selectionStart
                if (position < mEditText!!.text.length) {
                    val newText = mEditText!!.text.toString().substring(0, position) + " " +
                            mEditText!!.text.toString().substring(position)
                    mEditText!!.setText(newText)
                    mEditText!!.setSelection(position + 1)
                } else {
                    mEditText!!.append(" ")
                }
            }
            false
        }
        findViewById<View>(R.id.keyLeft).setOnTouchListener { _, event ->
            if (event.action == MotionEvent.ACTION_DOWN) {
                val position = mEditText!!.selectionStart
                if (position > 0) {
                    mEditText!!.setSelection(position - 1)
                }
            }
            false
        }
        findViewById<View>(R.id.keyRight).setOnTouchListener { _, event ->
            if (event.action == MotionEvent.ACTION_DOWN) {
                val position = mEditText!!.selectionStart
                if (position < mEditText!!.text.length) {
                    mEditText!!.setSelection(position + 1)
                }
            }
            false
        }
        if (findViewById<View?>(R.id.keyNumbers) != null) {
            findViewById<View>(R.id.keyNumbers).setOnTouchListener { _, event ->
                if (event.action == MotionEvent.ACTION_DOWN) {
                    showKeyboardType(KeyboardType.Num)
                }
                false
            }
        }
        if (findViewById<View?>(R.id.keyAbc) != null) {
            findViewById<View>(R.id.keyAbc).setOnTouchListener { _, event ->
                if (event.action == MotionEvent.ACTION_DOWN) {
                    showKeyboardType(KeyboardType.Abc)
                }
                false
            }
        }
    }

    private fun addLetterKeyHandlersForViewGroup(
        viewGroup: ViewGroup,
        isShifted: Boolean
    ) {
        for (i in 0 until viewGroup.childCount) {
            val child = viewGroup.getChildAt(i)
            if (child is ViewGroup) {
                addLetterKeyHandlersForViewGroup(child, isShifted)
            } else if (child is Button) {
                if ("key_letter" == child.getTag()) {
                    val key = child
                    key.setOnTouchListener { _, event ->
                        if (event.action == MotionEvent.ACTION_DOWN) {
                            val position = mEditText!!.selectionStart
                            if (position < mEditText!!.text.length) {
                                val newText = mEditText!!.text.toString().substring(0, position) +
                                        key.text.toString() +
                                        mEditText!!.text.toString().substring(position)
                                mEditText!!.setText(newText)
                                mEditText!!.setSelection(position + 1)
                            } else {
                                mEditText!!.append(key.text.toString())
                            }
                        }
                        false
                    }
                    setKeyCaseForButton(key, isShifted)
                }
            }
        }
    }

    private fun setKeyCase(isShifted: Boolean) {
        mIsShifted = isShifted
        val layout = findViewById<ViewGroup>(R.id.vr_keyboard)
        setKeyCaseForViewGroup(layout, isShifted)
    }

    companion object {
        private fun setKeyCaseForViewGroup(viewGroup: ViewGroup, isShifted: Boolean) {
            for (i in 0 until viewGroup.childCount) {
                val child = viewGroup.getChildAt(i)
                if (child is ViewGroup) {
                    setKeyCaseForViewGroup(child, isShifted)
                } else if (child is Button && "key_letter" == child.getTag()) {
                    setKeyCaseForButton(child, isShifted)
                }
            }
        }

        private fun setKeyCaseForButton(button: Button, isShifted: Boolean) {
            val text = button.text.toString()
            if (isShifted) {
                button.text = text.uppercase(Locale.getDefault())
            } else {
                button.text = text.lowercase(Locale.getDefault())
            }
        }
    }
}