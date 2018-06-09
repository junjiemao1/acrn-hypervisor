# Copyright (C) 2018 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# Given a Kconfig, this script show a ncurses-based TUI for modifying the
# configurations in the given .config.

import sys, os

# Kconfiglib: Copyright (c) 2011-2018, Ulf Magnusson
# SPDX-License-Identifier: ISC
# Refer to scripts/kconfig/LICENSE.kconfiglib for the permission notice.
import kconfiglib
import menuconfig

def usage():
    sys.stdout.write("%s: <Kconfig file> <.config file>\n" % sys.argv[0])

def main():
    if len(sys.argv) < 3:
        usage()
        sys.exit(1)

    kconfig_path = sys.argv[1]
    if not os.path.isfile(kconfig_path):
        sys.stderr.write("Cannot find file %s\n" % kconfig_path)
        sys.exit(1)

    kconfig = kconfiglib.Kconfig(kconfig_path)
    config_path = sys.argv[2]
    os.environ["KCONFIG_CONFIG"] = config_path

    menuconfig.menuconfig(kconfig)

if __name__ == "__main__":
    main()
