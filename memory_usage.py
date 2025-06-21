#!/usr/bin/env python3
import json
import os
import re
import subprocess
import sys

def extract_json_from_output(output: str) -> str | None:
    # Simple greedy regex to extract JSON object
    pattern = r'\{.*\}'
    match = re.search(pattern, output, re.DOTALL)
    if match:
        return match.group(0)
    return None

def bytes_to_kb(b):
    return b / 1024

def pct(used, total):
    if total == 0:
        return 0.0
    return (used / total) * 100

def main():
    build_dir = "build"
    desc_json = os.path.join(build_dir, "project_description.json")
    sdkconfig_json = os.path.join(build_dir, "config", "sdkconfig.json")
    
    # Check required files
    for f in [desc_json, sdkconfig_json]:
        if not os.path.isfile(f):
            print(f"Error: Missing {f}")
            sys.exit(1)
    
    # Load project description
    with open(desc_json) as f:
        desc = json.load(f)
    project_name = desc.get("project_name", "unknown")
    target_chip = desc.get("target", "unknown")
    
    # Load sdkconfig
    with open(sdkconfig_json) as f:
        sdkcfg = json.load(f)
    flash_size_str = sdkcfg.get("ESPTOOLPY_FLASHSIZE", "0MB")
    
    # Convert flash size string to bytes
    flash_size_map = {
        "1MB": 1 * 1024 * 1024,
        "2MB": 2 * 1024 * 1024,
        "4MB": 4 * 1024 * 1024,
        "8MB": 8 * 1024 * 1024,
        "16MB": 16 * 1024 * 1024,
        "32MB": 32 * 1024 * 1024,
        "64MB": 64 * 1024 * 1024,
        "128MB": 128 * 1024 * 1024,
    }
    flash_max = flash_size_map.get(flash_size_str, 0)

    # Parse RAM limits from .map file with attribute filtering
    map_file = os.path.join(build_dir, f"{project_name}.map")
    if not os.path.isfile(map_file):
        print(f"Error: Missing map file {map_file}")
        sys.exit(1)

    iram_max = 0
    dram_max = 0
    rtc_fast_max = 0

    in_mem_section = False
    with open(map_file) as f:
        for line in f:
            line = line.strip()
            if line.startswith("Memory Configuration"):
                in_mem_section = True
                # Skip header line after this
                next(f)  # skip "Name Origin Length Attributes"
                continue
            if in_mem_section:
                if line == "":
                    # empty line ends memory config section
                    break
                parts = re.split(r'\s+', line)
                if len(parts) < 4:
                    continue
                name, origin_str, length_str, attr = parts[:4]
                if not length_str.startswith("0x"):
                    continue
                try:
                    length = int(length_str, 16)
                except Exception:
                    continue
                # Sum only iram0_0_seg for internal IRAM
                if name == "iram0_0_seg" and "x" in attr and "r" in attr:
                    iram_max += length
                elif name.startswith("dram") and "r" in attr and "w" in attr:
                    dram_max += length
                elif name.startswith("rtc") and all(c in attr for c in "xrw"):
                    rtc_fast_max += length


    # Run idf.py size-components with JSON output
    try:
        proc = subprocess.run(
            ["idf.py", "size-components", "--format", "json"],
            check=True,
            capture_output=True,
            text=True,
        )
    except Exception as e:
        print(f"Error running idf.py: {e}")
        sys.exit(1)

    out = proc.stdout

    json_str = extract_json_from_output(out)
    if json_str is None:
        print("❌ Failed to find JSON output in idf.py size-components output")
        sys.exit(1)

    try:
        size_data = json.loads(json_str)
    except Exception as e:
        print(f"Failed to parse JSON data: {e}")
        sys.exit(1)

    # Helper functions to sum memory sections
    def extract_sum(pattern):
        total = 0
        for comp in size_data.values():
            for k, v in comp.items():
                if k == pattern:
                    total += v
        return total

    def extract_sum_prefix(prefix):
        total = 0
        for comp in size_data.values():
            for k, v in comp.items():
                if k.startswith(prefix):
                    total += v
        return total

    # Flash sections
    flash_rodata = extract_sum_prefix(".flash.rodata")
    flash_text = extract_sum_prefix(".flash.text")
    flash_appdesc = extract_sum(".flash.appdesc")
    flash_bin_hdr = extract_sum(".flash.bin_hdr")
    flash_misc = extract_sum(".flash.panic_info.desc")
    flash_total = extract_sum("flash_total")

    # RAM sections
    dram_data = extract_sum(".dram0.data")
    dram_bss = extract_sum(".dram0.bss")
    iram_text = extract_sum(".iram0.text")
    iram_vectors = extract_sum(".iram0.vectors")
    rtc_fast = extract_sum_prefix(".rtc_fast")

    iram_total = iram_text + iram_vectors
    dram_total = dram_data + dram_bss
    diram_used = iram_total + dram_total

    # Print report
    print("========== Target Info ==========")
    print(f"Project        : {project_name}")
    print(f"Target Chip    : {target_chip}")
    print(f"Flash Capacity : {flash_size_str} ({flash_max} bytes)")
    print()
    print(f"Flash              : {flash_total} bytes / {flash_max} bytes ({pct(flash_total, flash_max):.2f}%)")
    print(f"  ▸ .flash.text    : {flash_text} bytes")
    print(f"  ▸ .flash.rodata  : {flash_rodata} bytes")
    print(f"  ▸ .flash.appdesc : {flash_appdesc} bytes")
    print(f"  ▸ .flash.bin_hdr : {flash_bin_hdr} bytes")
    print(f"  ▸ .flash.misc    : {flash_misc} bytes")
    print()
    print(f"RAM (internal)     : {diram_used} bytes / {dram_max} bytes ({pct(diram_used, dram_max):.2f}%)")
    print(f"  ▸ .dram0.data    : {dram_data} bytes")
    print(f"  ▸ .dram0.bss     : {dram_bss} bytes")
    print(f"  ▸ .iram0.text    : {iram_text} bytes")
    print(f"  ▸ .iram0.vectors : {iram_vectors} bytes")
    print()
    print(f"RTC FAST Memory: {rtc_fast} bytes / {rtc_fast_max} bytes ({pct(rtc_fast, rtc_fast_max):.2f}%)")

if __name__ == "__main__":
    main()
