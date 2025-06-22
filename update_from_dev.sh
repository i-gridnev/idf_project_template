#!/usr/bin/env bash

set -euo pipefail

# CONFIGURATION
TEMPLATE_DIR="{{cookiecutter.project_name}}"
DEV_BRANCH="dev"
TMP_BRANCH="tmp_subtree_merge"
MAIN_BRANCH="main"

# Ensure we're on main
echo "→ Checking out '$MAIN_BRANCH'..."
git checkout "$MAIN_BRANCH"

# Clean up old temp branch if exists
if git show-ref --verify --quiet refs/heads/$TMP_BRANCH; then
    echo "→ Deleting existing temp branch '$TMP_BRANCH'..."
    git branch -D "$TMP_BRANCH"
fi

# Create temp branch
echo "→ Creating '$TMP_BRANCH' from '$MAIN_BRANCH'..."
git checkout -b "$TMP_BRANCH"

# Remove existing template folder if present
if [ -d "$TEMPLATE_DIR" ]; then
    echo "→ Removing existing '$TEMPLATE_DIR'..."
    git rm -r "$TEMPLATE_DIR"
    git commit -m "Remove old template folder"
else
    echo "ℹ️  No existing '$TEMPLATE_DIR' folder found, skipping removal"
fi

# Add subtree (squashed)
echo "→ Adding subtree from '$DEV_BRANCH' under '$TEMPLATE_DIR'..."
git subtree add --prefix="$TEMPLATE_DIR" "$DEV_BRANCH" --squash -m "Grab subtree"

# Squash everything in temp into one commit
echo "→ Squashing temp changes..."
git reset --soft "$MAIN_BRANCH"
git commit -m "Squashed update"

# Checkout back to main
echo "→ Switching back to '$MAIN_BRANCH'..."
git checkout "$MAIN_BRANCH"

# Cherry-pick into main without auto-committing
echo "→ Cherry-picking temp changes (no commit)..."
git cherry-pick "$TMP_BRANCH" --no-commit

# Delete temp branch
echo "→ Cleaning up temp branch..."
git branch -D "$TMP_BRANCH"

echo "✅ All done!"
echo "📝 Now review and commit manually"