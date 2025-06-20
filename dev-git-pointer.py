import os
import sys
import subprocess

TMP_DIR = "tmp"
GIT_DIR = "../.git"
GIT_FILE_PATH = os.path.join(TMP_DIR, ".git")

def get_current_branch():
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            capture_output=True, text=True, check=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        return None

def main():
    branch = get_current_branch()
    if branch != "dev":
        print(f"⚠️ Current branch is '{branch}'. This script runs only on 'dev' branch.")
        sys.exit(0)

    if not os.path.isdir(TMP_DIR):
        print(f"❌ Error: '{TMP_DIR}/' directory does not exist. Did you render the template?")
        sys.exit(1)

    with open(GIT_FILE_PATH, "w") as f:
        f.write(f"gitdir: {GIT_DIR}\n")

    print(f"✅ Created/overwritten '{GIT_FILE_PATH}' pointing to '{GIT_DIR}'")

if __name__ == "__main__":
    main()