package org.citra.citra_emu.vr;

public class ErrorMessageLayer
{

    public static ErrorMessageLayer instance = null;

    public static void showErrorWindow(final String titleStr,
                                       final String mainMessageStr)
    {
    }

    public void _showErrorWindow(final String titleStr,
                                 final String mainMessageStr)
    {
    }

    public void hideErrorWindow() {}

    private native void showErrorWindow(final boolean shouldShowError);
}
