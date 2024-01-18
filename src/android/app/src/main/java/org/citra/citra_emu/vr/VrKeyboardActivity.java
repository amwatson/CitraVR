package org.citra.citra_emu.vr;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputFilter;
import android.text.Spanned;
import android.text.TextWatcher;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContract;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.DialogFragment;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import java.io.Serializable;
import java.util.Objects;
import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.R;
import org.citra.citra_emu.applets.SoftwareKeyboard;
import org.citra.citra_emu.utils.Log;

public class VrKeyboardActivity extends android.app.Activity {

    private static final String EXTRA_KEYBOARD_INPUT_CONFIG =
        "org.citra.citra_emu.vr.KEYBOARD_INPUT_CONFIG";
    private static final String EXTRA_KEYBOARD_RESULT = "org.citra.citra_emu.vr.KEYBOARD_RESULT";

    public static class Result implements Serializable {
        public static enum Type { None, Positive, Neutral, Negative }
        ;
        public Result() {
            text = "";
            type = Type.None;
            config = null;
        }
        public Result(final String text, final Type type,
                      final SoftwareKeyboard.KeyboardConfig config) {
            this.text = text;
            this.type = type;
            this.config = config;
        }

        public Result(final Type type) {
            this.text = "";
            this.type = type;
            this.config = null;
        }

        public String text;
        public Type type;
        public SoftwareKeyboard.KeyboardConfig config;
    }

    public static class Contract
        extends ActivityResultContract<SoftwareKeyboard.KeyboardConfig, Result> {
        @Override
        public Intent createIntent(Context context, final SoftwareKeyboard.KeyboardConfig config) {
            Intent intent = new Intent(context, VrKeyboardActivity.class);
            intent.putExtra(EXTRA_KEYBOARD_INPUT_CONFIG, config);
            return intent;
        }

        @Override
        public Result parseResult(int resultCode, Intent intent) {
            if (resultCode != Activity.RESULT_OK) {
                Log.warning("parseResult(): Unexpected result code: " + resultCode);
                return new Result();
            }
            if (intent != null) {
                final Result result = (Result)intent.getSerializableExtra(EXTRA_KEYBOARD_RESULT);
                if (result != null) {
                    return result;
                }
            }
            Log.warning("parseResult(): finished with OK, but no result. Intent: " + intent);
            return new Result();
        }
    }

    public static void onFinishResult(final Result result) {
        switch (result.type) {
        case Positive:
            SoftwareKeyboard.onFinishVrKeyboardPositive(result.text, result.config);
            break;
        case Neutral:
            SoftwareKeyboard.onFinishVrKeyboardNeutral();
            break;
        case Negative:
        case None:
            SoftwareKeyboard.onFinishVrKeyboardNegative();
            break;
        }
    }

    private static enum KeyboardType { None, Abc, Num }

    private EditText mEditText = null;
    private boolean mIsShifted = false;
    private KeyboardType mKeyboardTypeCur = KeyboardType.None;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Bundle extras = getIntent().getExtras();
        SoftwareKeyboard.KeyboardConfig config = new SoftwareKeyboard.KeyboardConfig();
        if (extras != null) {
            config = (SoftwareKeyboard.KeyboardConfig)extras.getSerializable(
                EXTRA_KEYBOARD_INPUT_CONFIG);
        }

        setContentView(R.layout.vr_keyboard);
        mEditText = findViewById(R.id.vrKeyboardText);

        {
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            params.leftMargin = params.rightMargin =
                CitraApplication.getAppContext().getResources().getDimensionPixelSize(
                    R.dimen.dialog_margin);
            mEditText.setHint(config.hint_text);
            mEditText.setSingleLine(!config.multiline_mode);
            mEditText.setLayoutParams(params);
            mEditText.setFilters(
                new InputFilter[] {new SoftwareKeyboard.Filter(),
                                   new InputFilter.LengthFilter(config.max_text_length)});
        }

        // Needed to show cursor onscreen.
        mEditText.requestFocus();
        WindowCompat.getInsetsController(getWindow(), mEditText)
            .show(WindowInsetsCompat.Type.ime());

        setupResultButtons(config);
        showKeyboardType(KeyboardType.Abc);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (!hasFocus) {
            finish(); // Finish the activity when it loses focus, like an AlertDialog.
        }
    }

    private void setupResultButtons(final SoftwareKeyboard.KeyboardConfig config) {
        // Configure the result buttons
        findViewById(R.id.keyPositive).setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    Intent resultIntent = new Intent();
                    resultIntent.putExtra(
                        EXTRA_KEYBOARD_RESULT,
                        new Result(mEditText.getText().toString(), Result.Type.Positive, config));
                    setResult(Activity.RESULT_OK, resultIntent);
                    finish();
                }
                return false;
            }
        });

        findViewById(R.id.keyNeutral).setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    Intent resultIntent = new Intent();
                    resultIntent.putExtra(EXTRA_KEYBOARD_RESULT, new Result(Result.Type.Neutral));
                    setResult(Activity.RESULT_OK, resultIntent);
                    finish();
                }
                return false;
            }
        });

        findViewById(R.id.keyNegative).setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    Intent resultIntent = new Intent();
                    resultIntent.putExtra(EXTRA_KEYBOARD_RESULT, new Result(Result.Type.Negative));
                    setResult(Activity.RESULT_OK, resultIntent);
                    finish();
                }
                return false;
            }
        });

        switch (config.button_config) {
        case SoftwareKeyboard.ButtonConfig.Triple:
            findViewById(R.id.keyNeutral).setVisibility(View.VISIBLE);
            // fallthrough
        case SoftwareKeyboard.ButtonConfig.Dual:
            findViewById(R.id.keyNegative).setVisibility(View.VISIBLE);
            // fallthrough
        case SoftwareKeyboard.ButtonConfig.Single:
            findViewById(R.id.keyPositive).setVisibility(View.VISIBLE);
            // fallthrough
        case SoftwareKeyboard.ButtonConfig.None:
            break;
        default:
            Log.error("Unknown button config: " + config.button_config);
            assert false;
        }
    }

    private void showKeyboardType(final KeyboardType keyboardType) {
        if (mKeyboardTypeCur == keyboardType) {
            return;
        }
        mKeyboardTypeCur = keyboardType;
        final ViewGroup keyboard = findViewById(R.id.vr_keyboard_keyboard);
        keyboard.removeAllViews();
        switch (keyboardType) {
        case Abc:
            getLayoutInflater().inflate(R.layout.vr_keyboard_abc, keyboard);
            addLetterKeyHandlersForViewGroup(keyboard, mIsShifted);
            break;
        case Num:
            getLayoutInflater().inflate(R.layout.vr_keyboard_123, keyboard);
            addLetterKeyHandlersForViewGroup(keyboard, false);
            break;
        default:
            assert false;
        }
        addModifierKeyHandlers();
    }

    private void addModifierKeyHandlers() {
        findViewById(R.id.keyShift).setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    setKeyCase(!mIsShifted);
                }
                return false;
            }
        });
        // Note: I prefer touch listeners over click listeners because they activate
        // on the press instead of the release and therefore feel more responsive.
        findViewById(R.id.keyBackspace).setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    final String text = mEditText.getText().toString();
                    if (text.length() > 0) {
                        // Delete character before cursor
                        final int position = mEditText.getSelectionStart();
                        if (position > 0) {
                            final String newText =
                                text.substring(0, position - 1) + text.substring(position);
                            mEditText.setText(newText);
                            mEditText.setSelection(position - 1);
                        }
                    }
                }
                return false;
            }
        });

        findViewById(R.id.keySpace).setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    final int position = mEditText.getSelectionStart();
                    if (position < mEditText.getText().length()) {
                        final String newText =
                            mEditText.getText().toString().substring(0, position) + " " +
                            mEditText.getText().toString().substring(position);
                        mEditText.setText(newText);
                        mEditText.setSelection(position + 1);
                    } else {
                        mEditText.append(" ");
                    }
                }
                return false;
            }
        });

        findViewById(R.id.keyLeft).setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    final int position = mEditText.getSelectionStart();
                    if (position > 0) {
                        mEditText.setSelection(position - 1);
                    }
                }
                return false;
            }
        });

        findViewById(R.id.keyRight).setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    final int position = mEditText.getSelectionStart();
                    if (position < mEditText.getText().length()) {
                        mEditText.setSelection(position + 1);
                    }
                }
                return false;
            }
        });

        if (findViewById(R.id.keyNumbers) != null) {
            findViewById(R.id.keyNumbers).setOnTouchListener(new View.OnTouchListener() {
                @Override
                public boolean onTouch(View v, MotionEvent event) {
                    if (event.getAction() == MotionEvent.ACTION_DOWN) {
                        showKeyboardType(KeyboardType.Num);
                    }
                    return false;
                }
            });
        }
        if (findViewById(R.id.keyAbc) != null) {
            findViewById(R.id.keyAbc).setOnTouchListener(new View.OnTouchListener() {
                @Override
                public boolean onTouch(View v, MotionEvent event) {
                    if (event.getAction() == MotionEvent.ACTION_DOWN) {
                        showKeyboardType(KeyboardType.Abc);
                    }
                    return false;
                }
            });
        }
    }

    private void addLetterKeyHandlersForViewGroup(final ViewGroup viewGroup,
                                                  final boolean isShifted) {
        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            final View child = viewGroup.getChildAt(i);
            if (child instanceof ViewGroup) {
                addLetterKeyHandlersForViewGroup((ViewGroup)child, isShifted);
            } else if (child instanceof Button) {
                if ("key_letter".equals(child.getTag())) {
                    final Button key = (Button)child;
                    key.setOnTouchListener(new View.OnTouchListener() {
                        @Override
                        public boolean onTouch(View v, MotionEvent event) {
                            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                                final int position = mEditText.getSelectionStart();
                                if (position < mEditText.getText().length()) {
                                    final String newText =
                                        mEditText.getText().toString().substring(0, position) +
                                        key.getText().toString() +
                                        mEditText.getText().toString().substring(position);
                                    mEditText.setText(newText);
                                    mEditText.setSelection(position + 1);
                                } else {
                                    mEditText.append(key.getText().toString());
                                }
                            }
                            return false;
                        }
                    });
                    setKeyCaseForButton(key, isShifted);
                }
            }
        }
    }

    private void setKeyCase(final boolean isShifted) {
        mIsShifted = isShifted;
        final ViewGroup layout = findViewById(R.id.vr_keyboard);
        setKeyCaseForViewGroup(layout, isShifted);
    }

    private static void setKeyCaseForViewGroup(ViewGroup viewGroup, final boolean isShifted) {
        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            final View child = viewGroup.getChildAt(i);
            if (child instanceof ViewGroup) {
                setKeyCaseForViewGroup((ViewGroup)child, isShifted);
            } else if (child instanceof Button && "key_letter".equals(child.getTag())) {
                setKeyCaseForButton((Button)child, isShifted);
            }
        }
    }

    private static void setKeyCaseForButton(Button button, final boolean isShifted) {
        final String text = button.getText().toString();
        if (isShifted) {
            button.setText(text.toUpperCase());
        } else {
            button.setText(text.toLowerCase());
        }
    }
}
