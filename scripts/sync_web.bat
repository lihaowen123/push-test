@echo off
@REM .\scripts\sync_web.bat release-v6.0.3 D:\Jenkins\workspace\Community 
set web_tag_file=%cd%/web_sync_tag
if [%1]==[] (
    set web_branch=release-v6.0.3
)  else (
    set web_branch=%1
)
if [%2]==[] (
    set web_root=%cd%\..\Community\CrealityCommunity
) else (
    set web_root=%2
)
@REM if [%3]==[] (
@REM     set cp_branch=feature/syncweb
@REM ) else (
@REM     set cp_tmpval=%3
@REM     set "cp_branch=%cp_tmpval:origin/=%"
@REM )

set cp_source=%cd%
echo web_root=%web_root%
echo cp_source=%cp_source%
if not exist %web_root% (
    echo %web_root% not exist
    exit 1
)

@REM 定义源目录和目标目录
set "sync_source_dir=%web_root%\Community\dist"
set "sync_target_dir=%cp_source%\resources\web\homepage"
:READ_WEB_REPO_NUM
@REM read web repo version
set /p WEB_TAG_NUM=<%web_tag_file%
if "%WEB_TAG_NUM%"=="" (
    set WEB_TAG_NUM=0
)

:CP_RESET
echo CP_RESET
cd /d %cp_source%
@REM reset hard
echo reset hard cp
git reset --hard

:WEB_REPO
@REM web repo start
echo WEB_REPO start
echo curwebdir=%web_root%
cd /d %web_root%
git fetch
git checkout %web_branch%
git reset --hard 
git pull origin %web_branch%
REM 获取 git rev-list 的输出
for /f "tokens=*" %%i in ('git rev-list %web_branch% --count') do set GIT_COUNT=%%i
@REM get web git count
echo Git count: %GIT_COUNT%
echo Web tag num: %WEB_TAG_NUM%

if %GIT_COUNT% equ %WEB_TAG_NUM% (
    echo Count is equal to %WEB_TAG_NUM%, need not to run web build.
    goto SYNC_WEB
) else (
    echo Count is not equal to %WEB_TAG_NUM%
    goto RUN_WEB_TOOL
)

:RUN_WEB_TOOL
@REM run web build
cd %web_root%\Community
rmdir /s /q "%sync_source_dir%"
call npm run release || exit /b 1
echo npm run release end

rem copy start
:SYNC_WEB
echo SYNC_WEB Start
echo %GIT_COUNT% > %web_tag_file%
if not exist %sync_source_dir% (
    echo %sync_source_dir% not exist
    exit /b 1
)
echo copy start
xcopy "%sync_source_dir%\*" "%sync_target_dir%\" /s /e /y
echo  copy end

:END
cd  %cp_source%
echo web sync end



