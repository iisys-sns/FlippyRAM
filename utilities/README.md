# Utilities used for ARHE Project
Here you can find useful scripts that may help you for with the further development of ARHE or other projects.

# Current Scripts
- `find_paths_for_whitelist.py` script will help you to find all paths that you have defined in your `.sh` scripts. This is useful if you have created a lot of shell scripts with different paths and need to quickly grab and store the paths you have written so far which may be used later for whitelisting or other purposes. The script excludes the `data/tmp` path. It only retrieves the paths that have `/data` or `/data` is inside a variable e.g. `$var/example.log` where `var=$root_path/data/logs`
- `default_mat_gen.py` script is the default script which was written for `blacksmith PoC` to inject addressing functions. You can use this script to create your own customized addressing function injection. **ATTENTION: Do not modify `default_mat_gen.py` in this path. Copy it to your directory and modify it for your project**