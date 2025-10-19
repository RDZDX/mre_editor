"C:\Program Files\MRE_SDK\tools\DllPackage.exe" "E:\MyGitHub\mre_editor\mre_editor.vcproj"
if %errorlevel% == 0 (
 echo postbuild OK.
  copy mre_editor.vpp ..\..\..\MoDIS_VC9\WIN32FS\DRIVE_E\mre_editor.vpp /y
exit 0
)else (
echo postbuild error
  echo error code: %errorlevel%
  exit 1
)

