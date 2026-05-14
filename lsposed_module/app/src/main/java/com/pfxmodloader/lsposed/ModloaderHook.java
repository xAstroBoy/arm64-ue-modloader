package com.pfxmodloader.lsposed;

import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.callbacks.XC_LoadPackage;

/**
 * LSPosed module — injects libmodloader.so into any Unreal Engine app that is
 * selected in LSPosed scope.
 *
 * Path is shared and update-friendly:
 * /sdcard/UnrealModloader/loader/libmodloader.so
 *
 * One-process guard prevents duplicate loads in the same process.
 */
public class ModloaderHook implements IXposedHookLoadPackage {

    private static final String TAG = "UEModLoader/LSPosed";
    private static final String LIB_PATH = "/sdcard/UnrealModloader/loader/libmodloader.so";
    private static final Set<String> LOADED_PROCESSES = ConcurrentHashMap.newKeySet();

    private static boolean isUnrealPackage(XC_LoadPackage.LoadPackageParam lpparam) {
        if (lpparam.classLoader == null)
            return false;
        try {
            Class.forName("com.epicgames.unreal.GameActivity", false, lpparam.classLoader);
            return true;
        } catch (Throwable ignored) {
        }
        try {
            Class.forName("com.epicgames.ue4.GameActivity", false, lpparam.classLoader);
            return true;
        } catch (Throwable ignored) {
        }
        return false;
    }

    @Override
    public void handleLoadPackage(XC_LoadPackage.LoadPackageParam lpparam) {
        // LSPosed already limits scope to user-selected packages.
        if (!isUnrealPackage(lpparam)) {
            return;
        }

        String pkg = lpparam.packageName;
        String process = lpparam.processName != null ? lpparam.processName : pkg;
        String key = pkg + "::" + process;
        if (LOADED_PROCESSES.contains(key))
            return;

        XposedBridge.log(TAG + " [" + pkg + "]: loading " + LIB_PATH);
        try {
            System.load(LIB_PATH);
            LOADED_PROCESSES.add(key);
            XposedBridge.log(TAG + " [" + pkg + "]: Loaded");
        } catch (Throwable t) {
            XposedBridge.log(TAG + " [" + pkg + "]: FAILED: " + t);
        }
    }
}
