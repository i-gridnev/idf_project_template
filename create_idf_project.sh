#!/bin/sh
usage_help() {
 echo "Usage: $0 [project_name] [project_path]"
}

if [ -n "$1" ]
then
proj_name=$1
else
usage_help
exit 1
fi

if [ -n "$2" ]
then
proj_path=$2/$proj_name
else
usage_help
exit 1
fi

template_dir="$(dirname "$(readlink -f "$0")")"
if [ -d $proj_path ]; then
  echo "ERROR: project directory already exist, check [project_name] [project_path]"
  exit 1
fi

cp -r $template_dir $proj_path
rm -rf $proj_path/.git
rm $proj_path/create_idf_project.sh

sed -i "s/*project_name/$proj_name/" $proj_path/.devcontainer/devcontainer.json
sed -i "s/*project_name/$proj_name/" $proj_path/CMakeLists.txt

echo "SUCCESS: project initialized in: $proj_path";
echo "Project started in VS Code tab (Use Reopen in Container)";

code $proj_path