@echo off
echo ==> Pulling latest from origin...
git pull
echo ==> Fetching LFS assets...
git lfs pull
echo Done. Now open SiteSyncAR\SiteSyncAR.uproject in UE5.
echo.
echo REMINDER when you finish working:
echo   1. Save All in UE5 (Ctrl+Shift+S)
echo   2. git add SiteSyncAR/Content/ SiteSyncAR/Source/ SiteSyncAR/Config/
echo   3. git commit -m "feat: ..."
echo   4. git push
pause
