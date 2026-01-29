#!/usr/bin/env python3
"""
Add Power Configuration Cluster (0x0001) to ZCL configuration
This script adds battery monitoring attributes for 2xAAA configuration.
"""

import json
import sys
from pathlib import Path

# Power Configuration cluster definition
POWER_CONFIG_CLUSTER = {
    "name": "Power Configuration",
    "code": 1,
    "mfgCode": None,
    "define": "POWER_CONFIGURATION_CLUSTER",
    "side": "server",
    "enabled": 1,
    "attributes": [
        {
            "name": "BatteryVoltage",
            "code": 32,  # 0x0020
            "mfgCode": None,
            "side": "server",
            "type": "int8u",
            "included": 1,
            "storageOption": "RAM",
            "singleton": 0,
            "bounded": 0,
            "defaultValue": "30",  # 3.0V nominal for 2xAAA
            "reportable": 1,
            "minInterval": 3600,  # 1 hour minimum
            "maxInterval": 7200,  # 2 hours maximum
            "reportableChange": 1  # Report on 100mV (0.1V) change
        },
        {
            "name": "BatteryPercentageRemaining",
            "code": 33,  # 0x0021
            "mfgCode": None,
            "side": "server",
            "type": "int8u",
            "included": 1,
            "storageOption": "RAM",
            "singleton": 0,
            "bounded": 0,
            "defaultValue": "200",  # 100% (200 = 100%)
            "reportable": 1,
            "minInterval": 3600,  # 1 hour minimum
            "maxInterval": 7200,  # 2 hours maximum
            "reportableChange": 10  # Report on 5% change (10/200 = 5%)
        },
        {
            "name": "BatterySize",
            "code": 49,  # 0x0031
            "mfgCode": None,
            "side": "server",
            "type": "enum8",
            "included": 1,
            "storageOption": "RAM",
            "singleton": 0,
            "bounded": 0,
            "defaultValue": "3",  # 0x03 = AAA
            "reportable": 0,
            "minInterval": 0,
            "maxInterval": 65344,
            "reportableChange": 0
        },
        {
            "name": "BatteryQuantity",
            "code": 51,  # 0x0033
            "mfgCode": None,
            "side": "server",
            "type": "int8u",
            "included": 1,
            "storageOption": "RAM",
            "singleton": 0,
            "bounded": 0,
            "defaultValue": "2",  # 2xAAA
            "reportable": 0,
            "minInterval": 0,
            "maxInterval": 65344,
            "reportableChange": 0
        },
        {
            "name": "BatteryRatedVoltage",
            "code": 53,  # 0x0035
            "mfgCode": None,
            "side": "server",
            "type": "int8u",
            "included": 1,
            "storageOption": "RAM",
            "singleton": 0,
            "bounded": 0,
            "defaultValue": "30",  # 3.0V nominal (in 100mV units)
            "reportable": 0,
            "minInterval": 0,
            "maxInterval": 65344,
            "reportableChange": 0
        },
        {
            "name": "cluster revision",
            "code": 65533,
            "mfgCode": None,
            "side": "server",
            "type": "int16u",
            "included": 1,
            "storageOption": "RAM",
            "singleton": 0,
            "bounded": 0,
            "defaultValue": "1",
            "reportable": 1,
            "minInterval": 0,
            "maxInterval": 65344,
            "reportableChange": 0
        }
    ]
}

def main():
    # Path to ZCL config file
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    zap_file = project_root / "config" / "zcl" / "zcl_config.zap"

    if not zap_file.exists():
        print(f"Error: ZAP config file not found: {zap_file}")
        sys.exit(1)

    print(f"Reading ZAP config from: {zap_file}")

    # Load JSON
    with open(zap_file, 'r') as f:
        zap_config = json.load(f)

    # Find endpoint type for sensor (should be endpoint 1)
    endpoint_types = zap_config.get("endpointTypes", [])
    if not endpoint_types:
        print("Error: No endpoint types found in ZAP config")
        sys.exit(1)

    sensor_endpoint = endpoint_types[0]  # First endpoint is our sensor
    clusters = sensor_endpoint.get("clusters", [])

    # Check if Power Configuration cluster already exists
    power_config_exists = any(c.get("code") == 1 for c in clusters)

    if power_config_exists:
        print("Power Configuration cluster already exists!")
        print("Updating existing cluster attributes...")

        # Update existing cluster
        for cluster in clusters:
            if cluster.get("code") == 1:
                cluster.update(POWER_CONFIG_CLUSTER)
                break
    else:
        print("Adding Power Configuration cluster (0x0001)...")

        # Insert Power Config after Basic (0x0000) and before Identify (0x0003)
        # Find the position
        insert_pos = 1  # After Basic
        for i, cluster in enumerate(clusters):
            if cluster.get("code") == 0:  # Basic cluster
                insert_pos = i + 1
                break

        clusters.insert(insert_pos, POWER_CONFIG_CLUSTER)

    # Write back to file
    print(f"Writing updated config to: {zap_file}")
    with open(zap_file, 'w') as f:
        json.dump(zap_config, f, indent=2)

    print("\nSuccess! Power Configuration cluster added/updated with:")
    print("  - BatteryVoltage (0x0020): Battery voltage in 100mV units")
    print("  - BatteryPercentageRemaining (0x0021): 0-200 (0.5% resolution)")
    print("  - BatterySize (0x0031): 0x03 (AAA)")
    print("  - BatteryQuantity (0x0033): 2 (2xAAA)")
    print("  - BatteryRatedVoltage (0x0035): 30 (3.0V)")
    print("\nConfiguration for 2xAAA batteries:")
    print("  - Full: 3.2V (2x 1.6V fresh alkaline)")
    print("  - Nominal: 3.0V (2x 1.5V)")
    print("  - Empty: 1.8V (2x 0.9V depleted)")
    print("\nReporting configured:")
    print("  - Voltage: Report every 1-2 hours or on 0.1V change")
    print("  - Percentage: Report every 1-2 hours or on 5% change")

if __name__ == "__main__":
    main()
