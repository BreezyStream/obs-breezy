Remove-Item -Path "build_x64" -Recurse
$env:CMAKE_POLICY_VERSION_MINIMUM = "3.5"
cmake --preset windows-x64 --fresh
cmake --build --preset windows-x64 --config Release