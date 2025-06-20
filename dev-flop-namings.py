import os
import shutil
import subprocess
import sys

DEV_BRANCH = "dev"

FOLDER_DEV_NAME = "dev_template"
FOLDER_TEMPLATE_NAME = "{{cookiecutter.project_name}}"
GIT_RELINK = "gitdir: ../.git\n"
GIT_PATH = os.path.join(FOLDER_DEV_NAME, ".git")

def get_current_branch():
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            capture_output=True, text=True, check=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        return None

def inject_git_pointer():
    os.makedirs(FOLDER_DEV_NAME, exist_ok=True)
    with open(GIT_PATH, "w") as f:
        f.write(GIT_RELINK)
    print(f"🔗 Injected .git file in '{FOLDER_DEV_NAME}/'")

def remove_git_pointer():
    if os.path.exists(GIT_PATH):
        os.remove(GIT_PATH)
        print(f"🧹 Removed .git file from '{FOLDER_DEV_NAME}/'")

def flip_folder():
    if os.path.isdir(FOLDER_TEMPLATE_NAME) and not os.path.exists(FOLDER_DEV_NAME):
        shutil.move(FOLDER_TEMPLATE_NAME, FOLDER_DEV_NAME)
        print(f"🔁 Renamed '{FOLDER_TEMPLATE_NAME}' → '{FOLDER_DEV_NAME}'")
        inject_git_pointer()
    elif os.path.isdir(FOLDER_DEV_NAME) and not os.path.exists(FOLDER_TEMPLATE_NAME):
        remove_git_pointer()
        shutil.move(FOLDER_DEV_NAME, FOLDER_TEMPLATE_NAME)
        print(f"🔁 Renamed '{FOLDER_DEV_NAME}' → '{FOLDER_TEMPLATE_NAME}'")
    else:
        print("❌ Cannot flip: ambiguous or conflicting state.")
        print(f"   Exists: {FOLDER_TEMPLATE_NAME}? {'yes' if os.path.exists(FOLDER_TEMPLATE_NAME) else 'no'}")
        print(f"   Exists: {FOLDER_DEV_NAME}? {'yes' if os.path.exists(FOLDER_DEV_NAME) else 'no'}")
        sys.exit(1)

def main():
    branch = get_current_branch()
    if branch != DEV_BRANCH:
        print(f"🚫 You are on branch '{branch}', not '{DEV_BRANCH}'. Flip allowed only in dev.")
        sys.exit(1)
    flip_folder()

if __name__ == "__main__":
    main()