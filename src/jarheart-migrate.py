#!/usr/bin/env python3
# jarheart-migrate -- Migrate legacy Redshift configurations to Jarheart format
# This file is part of Jarheart.
#
# Copyright (c) 2026 Jarheart Contributors
# Distributed under the GNU General Public License v3 or later.

import os
import sys
import shutil
import argparse
import configparser
from datetime import datetime

DEFAULT_TEMPLATE = """# Jarheart Configuration File
# Automatically generated / migrated by jarheart-migrate on {timestamp}

[jarheart]
; Color temperatures (Kelvin) for daytime and nighttime
temp-day={temp_day}
temp-night={temp_night}

; Transition curve and fading
fade=1
gamma={gamma}
brightness={brightness}

; Schedule mode: solar (astronomical elevation), diurnal (tri-phasic focus/comfort), or time
schedule={schedule}

; Evidence-based ocular health and ergonomic couplings
; Couple display luminance with color temperature along Kruithof comfort curve (55-60% at night)
couple-brightness={couple_brightness}

; Myopia protection mode (calibrated 2850K long-wavelength spectrum, 60% luminance clamp)
myopia-protect={myopia_protect}

; Ambient contrast balancer (scales screen brightness dynamically to match room lux)
ambient-balancer={ambient_balancer}

; 20-20-20 ocular rest pacer interval in seconds (1200 = 20m, 1800 = 30m, 0 = disabled)
pacer-interval={pacer_interval}

; Location and adjustment method providers
location-provider={location_provider}
adjustment-method={adjustment_method}

[manual]
lat={lat}
lon={lon}
"""

def find_legacy_config():
    home = os.path.expanduser("~")
    candidates = [
        os.path.join(home, ".config", "redshift.conf"),
        os.path.join(home, ".config", "redshift", "redshift.conf"),
        "/etc/redshift.conf"
    ]
    for p in candidates:
        if os.path.isfile(p):
            return p
    return None

def main():
    parser = argparse.ArgumentParser(description="Migrate legacy Redshift configurations to Jarheart")
    parser.add_argument("-s", "--source", help="Custom path to legacy redshift.conf")
    parser.add_argument("-d", "--dest", help="Custom destination path (default: ~/.config/jarheart/jarheart.conf)")
    parser.add_argument("-n", "--dry-run", action="store_true", help="Print migrated config without writing to disk")
    parser.add_argument("-f", "--force", action="store_true", help="Overwrite destination file if it already exists")
    args = parser.parse_args()

    home = os.path.expanduser("~")
    dest_path = args.dest if args.dest else os.path.join(home, ".config", "jarheart", "jarheart.conf")

    source_path = args.source if args.source else find_legacy_config()

    params = {
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "temp_day": "6500",
        "temp_night": "3400",
        "gamma": "1.0:1.0:1.0",
        "brightness": "1.0",
        "schedule": "solar",
        "couple_brightness": "true",
        "myopia_protect": "false",
        "ambient_balancer": "false",
        "pacer_interval": "1200",
        "location_provider": "manual",
        "adjustment_method": "randr",
        "lat": "41.8500",
        "lon": "-87.6500",
    }

    if source_path and os.path.isfile(source_path):
        print(f"Discovered legacy Redshift configuration at: {source_path}")
        cp = configparser.ConfigParser()
        try:
            cp.read(source_path)
            section = "redshift" if cp.has_section("redshift") else "DEFAULT"
            if cp.has_option(section, "temp-day"):
                params["temp_day"] = cp.get(section, "temp-day")
            if cp.has_option(section, "temp-night"):
                params["temp_night"] = cp.get(section, "temp-night")
            if cp.has_option(section, "gamma"):
                params["gamma"] = cp.get(section, "gamma")
            if cp.has_option(section, "brightness"):
                params["brightness"] = cp.get(section, "brightness")
            if cp.has_option(section, "location-provider"):
                params["location_provider"] = cp.get(section, "location-provider")
            if cp.has_option(section, "adjustment-method"):
                params["adjustment_method"] = cp.get(section, "adjustment-method")

            if cp.has_section("manual"):
                if cp.has_option("manual", "lat"):
                    params["lat"] = cp.get("manual", "lat")
                if cp.has_option("manual", "lon"):
                    params["lon"] = cp.get("manual", "lon")
        except Exception as e:
            print(f"Warning: could not parse all fields from {source_path}: {e}")
    else:
        print("No legacy redshift.conf found. Generating modern Jarheart defaults.")

    rendered = DEFAULT_TEMPLATE.format(**params)

    if args.dry_run:
        print("\n--- Migrated Configuration Preview ---")
        print(rendered)
        print("--- End Preview ---")
        return 0

    dest_dir = os.path.dirname(dest_path)
    if dest_dir and not os.path.exists(dest_dir):
        os.makedirs(dest_dir, exist_ok=True)

    if os.path.exists(dest_path) and not args.force:
        backup_path = dest_path + ".bak." + datetime.now().strftime("%Y%m%d%H%M%S")
        shutil.copy2(dest_path, backup_path)
        print(f"Existing Jarheart config backed up to: {backup_path}")

    with open(dest_path, "w") as f:
        f.write(rendered)

    print(f"Successfully generated Jarheart configuration at: {dest_path}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
