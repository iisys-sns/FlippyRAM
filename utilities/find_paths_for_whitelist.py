import os
import re

# IMPORTANT: This script is a beta and might have minor output problems therefore you might need to check the output yourself manually

# Directory to search (set your target directory here)
# Please move all your sh files to a directory and set it here
directory_to_search = "./example"

# Output file for found paths
output_file = "paths_found.txt"

# Regular expression to match paths and variable declarations
path_pattern = r"(?<!\w)(data(?:Dir)?(?:/[^\s]*)?)"
var_declaration_pattern = r'(\w+)=["\']([^"\']*data[^"\']*)["\']'
var_usage_pattern = r"(\$\w+)(/[^\s]*)?"

# Excluded path
excluded_path = "data/tmp"

# Set to hold unique paths
found_paths = set()
variable_paths = {}

# Function to classify path as directory or file
def classify_path(path):
    if path.endswith("/"):
        return "|directory"
    else:
        return "|text/plain"

# Function to resolve variables in paths
def resolve_variable_path(variable, extra_path=""):
    if variable in variable_paths:
        base_path = variable_paths[variable]
        return f"{base_path}{extra_path}"
    return None

# Function to process each .sh file
def process_sh_file(filepath):
    global found_paths
    with open(filepath, "r") as file:
        lines = file.readlines()
        for line in lines:
            # Match variable declarations
            var_match = re.match(var_declaration_pattern, line)
            if var_match:
                var_name, var_path = var_match.groups()
                variable_paths[f"${var_name}"] = var_path

            # Match standard paths
            path_match = re.search(path_pattern, line)
            if path_match:
                path = re.sub(r"[\"')]+$", "", path_match.group(1))
                if excluded_path not in path:
                    found_paths.add(path)

            # Match variable usages
            var_usage_match = re.search(var_usage_pattern, line)
            if var_usage_match:
                var_name, extra_path = var_usage_match.groups()
                resolved_path = resolve_variable_path(var_name, extra_path or "")
                if resolved_path and excluded_path not in resolved_path:
                    found_paths.add(resolved_path)

# Walk through the directory and subdirectories
for root, _, files in os.walk(directory_to_search):
    for file in files:
        if file.endswith(".sh"):
            process_sh_file(os.path.join(root, file))

# Write formatted paths to the output file
with open(output_file, "w") as f:
    for path in sorted(found_paths):
        classification = classify_path(path)
        f.write(f"{path}{classification}\n")

print(f"Paths have been written to {output_file}")
