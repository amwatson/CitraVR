#!/usr/bin/env python3
# Script to run android build commands in a more convenient way
# Note: replace `package` and `launch_activity` with your own
# Author: Amanda M. Watson (amwatson)

import os
import subprocess as sp
import sys

# ================
# Global constants
# ================

package = "org.citra.citra_emu"
launch_activity = "org.citra.citra_emu.ui.main.MainActivity"

# ========================
# Global mutable variables
# ========================

has_executed_build = False

# ================
# Helper functions
# ================

def shell_cmd(cmd):
    return sp.call(cmd.split())

def adb_shell_cmd(cmd):
    return shell_cmd(f"adb shell {cmd}")

def check_submodules():
    sm_status = sp.run("git submodule status", stdout=sp.PIPE, stderr=sp.PIPE, shell=True, text=True)
    if sm_status.stderr.strip():
        print(f"Error checking submodules: {status_error}")
        return status_error

    if any(line.startswith('-') for line in sm_status.stdout.strip().splitlines()):
        print("Submodule(s) not found -- updating submodules...")
        update_error = shell_cmd("git submodule update --init --recursive")

        if update_error:
            print(f"Error updating submodules: {update_error}")
            return update_error

        print("Submodules updated successfully.")

    print("All submodules are up to date.")
    return 0

# ==================
# Available commands
# ==================

def start(_):
    return adb_shell_cmd(f"am start {package}/{launch_activity}")

def stop(_):
    return adb_shell_cmd(f"am force-stop {package}")

def install(build_config):
    return (check_submodules() >= 0) and shell_cmd(f"./gradlew installNightly{build_config}")

def uninstall(build_config):
    return shell_cmd(f"./gradlew uninstallNightly{build_config}")

def build(build_config):
   return (check_submodules() >= 0) and shell_cmd(f"./gradlew assemble{build_config}")

def clean(_):
    return shell_cmd("./gradlew clean")

# Main

def main():
    argv = sys.argv[1:]
    if len(argv) == 0:
        print(
    """Usage: build.py [debug | profile | release] <options...>
    options:
        - build
        - install
        - uninstall
        - start
        - stop
        - clean
    Options will execute in order, e.g. cmd.py clean build install start stop uninstall""")
        exit(-1)

    build_configs = ["debug", "release"]
    active_build_config = "release"
    if (argv[0] in build_configs):
        active_build_config = argv[0]
        argv = argv[1:]

    for idx, arg in enumerate(argv):
        try:
            if (globals()[arg](active_build_config.capitalize()) != 0):
                if (arg == "install" and active_build_config == "release"):
                    print("**Warning: this command fails if a release keystore is not specified")
                if (len(argv[idx+1:]) > 0):
                    print("ERROR: The following commands were not executed: ", argv[idx+1:])
                    exit(-2)
                elif (arg == "build"):
                        has_executed_build = True
        except KeyError:
            print("Error: unrecognized command '%s'" % arg)
            exit(-3)

if __name__ == "__main__":
    main()
