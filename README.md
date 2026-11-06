# Stream Launcher V1.5.2

Windows 直播程式啟動器。以 C++17 與 Win32 重新製作，整合舊版的 OBS、VTube Studio、onecomme、Mix-it、RTMP 工具與 TwitchTest 啟動選項。執行時不需要 Python。

## VTube Studio 與 Steam

預設的「透過 Steam 啟動」會把 `steam://rungameid/1325860` 交給 Windows／Steam。請先安裝 Steam、登入帳號並安裝 VTube Studio；此模式不需要填 VTS exe 路徑。若 Steam 無法處理請求，程式會顯示錯誤，不會悄悄改用直接模式。

「直接執行 exe」沿用舊版做法，須填 VTube Studio 的 exe 路徑。VTS 官方說明指出，未經 Steam 啟動時 VNet 多人功能不可用；去浮水印 DLC 也可能需要先經 Steam 啟動一次。啟動器只能將請求交給 Steam，實際登入與授權狀態由 Steam 和 VTS 決定。

參考：[VTS 官方 Starting without Steam](https://github.com/DenchiSoft/VTubeStudio/wiki/Starting-without-Steam)、[VTube Studio Steam 頁面](https://store.steampowered.com/app/1325860/VTube_Studio/)。

## 使用

下載或建置後執行 `dist/StreamLauncher-V1.5.2/StreamLauncher.exe`。設定會保存在 exe 旁的 `settings.ini`，可參考 [`settings.example.ini`](settings.example.ini)。請將程式放在可寫入的資料夾。路徑可能含個人資訊，`settings.ini` 只保留在本機，不會提交 GitHub，也不會放進發布 ZIP；公開檔案只提供 `settings.example.ini`。如果已有舊版 `settings.ini`，可自行複製到新版 exe 旁；重新建置不會刪除它。更早版本的明亮／深色選擇與青綠色主色也能沿用。

使用「瀏覽」選定路徑後會立即儲存；手動修改路徑後離開欄位、切換勾選或啟動直播時也會儲存。主畫面保留 Steam／直接啟動切換，並提供獨立的 TwitchTest 啟動按鈕。「關於／設定」保留舊版資訊順序，可選跟隨設備／明亮／深色風格，以內建選色視窗拖曳選色或輸入 `#RRGGBB` 色碼、切換繁體中文／英文、檢查 GitHub 公開 Release，以及選擇是否在登入 Windows 時啟動。跟隨設備會讀取 Windows 的應用程式色彩偏好，並在系統設定變更時更新畫面。外觀與語言按「套用」後儲存；路徑仍會自動儲存。提示文字參考舊版的「成功／警告／錯誤」語氣，並保留詳細錯誤；啟動成功提示代表指令已送出，並不保證其他程式完成啟動。更新檢查只在按鈕點擊時執行；若目前版本比最新公開 Release 新，會直接說明，不會提示下載舊版。開機啟動只開啟啟動器視窗；OBS、VTS 與其他工具仍須手動按「準備直播」才會啟動。此功能使用目前使用者的 Windows `Run` 登錄項目，移動 exe 後需重新勾選以更新路徑。

OBS 與勾選的選用工具需填 exe 路徑。單一程式啟動失敗不會阻止其他程式，最後會列出錯誤。TwitchTest 放在 exe 旁的 `twitchtest-2.0/TwitchTest.exe`，沿用舊版的管理員啟動方式；TwitchTest 由 R1ch 提供，串流金鑰需由使用者自行填入且不可提交 Git。

舊版 Python 的 `config.json` 可用以下指令轉換；Self Edition 的 `RTMP_path` 與 `sync_RTMP` 也可讀取。請在第一次開啟新程式前，於倉庫根目錄執行並指定舊設定的完整路徑。若目的設定檔已存在，腳本會停止以免覆蓋：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/migrate-config.ps1 -Source "C:\old\config.json" -Destination "dist\StreamLauncher-V1.5.2\settings.ini"
```

轉換後開啟程式確認路徑與勾選狀態。舊版沒有 Steam 模式選項，轉換後預設透過 Steam 啟動。

## 開發與建置

需要 Windows 10/11 與 Visual Studio 2022 C++ Build Tools。於倉庫根目錄執行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```

腳本編譯 Win32 GUI、資源圖示與 C++ 測試，執行測試後把 exe、範例設定與 TwitchTest 複製到 `dist/StreamLauncher-V1.5.2/`。若已安裝 CMake，也可使用 [`CMakeLists.txt`](CMakeLists.txt) 建置與執行 CTest。`build/` 與 `dist/` 不提交 Git。

需要製作公開 ZIP 時，執行 `powershell -ExecutionPolicy Bypass -File scripts/package-public.ps1`。此腳本只打包指定檔案，會檢查 ZIP 不含 `settings.ini`。

主要程式碼位於 `src/main.cpp`（GUI）及 `src/launcher.cpp`（設定與啟動）。
