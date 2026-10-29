#!/usr/bin/env python3
import glob, sys
from pbxproj import XcodeProject

# Find your project; tweak the glob if your preset uses another dir/name
candidates = glob.glob('build_macos/*.xcodeproj/project.pbxproj') \
          or glob.glob('build/*.xcodeproj/project.pbxproj')
if not candidates:
    print("No project.pbxproj found under build_macos/ or build/. Adjust glob.")
    sys.exit(1)

path = candidates[0]
print(f"[strip-agl] Patching {path}")

proj = XcodeProject.load(path)

removed = 0
files = proj.get_files_by_name('AGL.framework')

for f in files:
    fid = f.get_id()
    name = f.get('name', None) or f.get('path', None) or fid  # <-- pass default
    print(f"[strip-agl] Removing {name} ({fid})")
    # Remove the file everywhere (build phases + groups + refs)
    proj.remove_file_by_id(fid)
    removed += 1

# Also scrub any leftover OTHER_LDFLAGS mentions just in case
for t in proj.objects.get_targets():
    for cfg in proj.objects.get_configurations_on_targets(t):
        bs = cfg.get('buildSettings', {})  # default dict if missing
        flags = bs.get('OTHER_LDFLAGS', [])
        if isinstance(flags, list):
            new_flags = [x for x in flags if x not in ('-framework', 'AGL') and 'AGL.framework' not in str(x)]
        else:
            new_flags = flags
        bs['OTHER_LDFLAGS'] = new_flags

if removed:
    proj.backup()  # writes a .backup
    proj.save()
    print(f"[strip-agl] Removed {removed} AGL.framework reference(s).")
else:
    print("[strip-agl] No AGL.framework references found. Nothing to do.")
