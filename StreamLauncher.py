import tkinter as tk
import json
import os
import subprocess
import sys
import requests

from tkinter import ttk, filedialog
from tkinter import messagebox

# 處裡打包造成路徑錯誤的問題

def resource_path(relative_path):
    """取得 PyInstaller 打包後的資源路徑"""
    try:
        # PyInstaller 打包後會有 _MEIPASS
        base_path = sys._MEIPASS
    except AttributeError:
        # 開發模式
        base_path = os.path.abspath(".")

    return os.path.join(base_path, relative_path)

# =========================
# 基本設定
# =========================

APP_NAME = "Stream Launcher"
CONFIG_FILE = resource_path("config.json")
APP_VERSION = "0.1"
GITHUB_REPO = "Joeeey0219/Stream-Software-Launcher"

# =========================
# 主視窗初始化
# =========================

root = tk.Tk()

# 設定標題列文字
root.title(APP_NAME)

# 最小大小
root.minsize(520, 360)

# 允許使用者調整大小
root.resizable(True, True)

# =========================
# 視窗主題（樣式）
# =========================

style = ttk.Style()

# 可用 'vista', 'xpnative', 'default', 'clam'
# 不同系統支援不同主題
current_theme = tk.StringVar(value="clam")

# =========================
# 視窗 Icon（左上角小圖示）
# =========================

# 把 icon.ico 放在同資料夾即可
root.iconbitmap(resource_path("icon/icon.ico"))

# =========================
# Tkinter 變數（綁定 UI 狀態）
# =========================

# StringVar → 存文字（路徑）
obs_path = tk.StringVar()
vts_path = tk.StringVar()
onecomm_path = tk.StringVar()

# BooleanVar → 存 True / False（勾選）
admin_obs = tk.BooleanVar()
sync_onecomm = tk.BooleanVar()
sync_twitch = tk.BooleanVar()

TWITCH_TEST_PATH = resource_path("twitchtest-2.0/TwitchTest.exe")

# =========================
# 功能性函式
# =========================

def check_update():
    """檢查 GitHub 上是否有新版本"""
    try:
        url = f"https://api.github.com/repos/{GITHUB_REPO}/releases/latest"
        response = requests.get(url, timeout=5)
        if response.status_code == 200:
            data = response.json()
            latest_version = data.get("tag_name")  # Release 標籤名稱
            release_url = data.get("html_url")
            
            if latest_version != APP_VERSION:
                if messagebox.askyesno(
                    "更新可用",
                    f"發現新版本 {latest_version} (目前 {APP_VERSION})\n是否前往下載？"
                ):
                    import webbrowser
                    webbrowser.open(release_url)
            else:
                return True # 已是最新版本
        else:
            return False # 無法取得 GitHub Release
    except Exception as e:
        messagebox.showerror(
        "錯誤", f"檢查更新失敗: {e}")

def check_update_GUI():
    """在 GUI 中檢查更新"""
    if check_update() == True:
        messagebox.showinfo(
        "已是最新版本", f"目前版本 {APP_VERSION} 已是最新！")
    elif check_update() == False:
        messagebox.showerror(
        "錯誤", "無法取得 GitHub Release")

def launch_obs():
    """啟動 OBS，支援管理員模式並指定工作目錄"""
    if not obs_path.get():
        messagebox.showwarning("警告", "OBS 路徑未設定")
        return False

    try:
        obs_exe = obs_path.get()
        obs_dir = os.path.dirname(obs_exe)  # 取得 exe 所在資料夾

        if admin_obs.get() and sys.platform == "win32":
            # 管理員模式 + 指定工作目錄
            subprocess.run(
                f'powershell -Command "Start-Process \\"{obs_exe}\\" -Verb RunAs -WorkingDirectory \\"{obs_dir}\\""',
                shell=True
            )
        else:
            # 一般模式，指定工作目錄
            subprocess.Popen([obs_exe], cwd=obs_dir)

        return True
    except Exception as e:
        messagebox.showerror("錯誤", f"OBS 啟動失敗：{e}")
        return False

def launch_vts():
    """啟動 VTube Studio"""
    if not vts_path.get():
        messagebox.showwarning("警告", "VTube Studio 路徑未設定")
        return False
    try:
        subprocess.Popen([vts_path.get()])
        return True
    except Exception as e:
        messagebox.showerror("錯誤", f"VTube Studio 啟動失敗：{e}")
        return False

def launch_onecomm():
    """啟動 OneCommond"""
    if not onecomm_path.get():
        messagebox.showwarning("警告", "OneCommond 路徑未設定")
        return False
    try:
        subprocess.Popen([onecomm_path.get()])
        return True
    except Exception as e:
        messagebox.showerror("錯誤", f"OneCommond 啟動失敗：{e}")
        return False

def launch_twitch_test():
    """啟動 Twitch 測試軟體"""
    try:
        test_dir = os.path.dirname(TWITCH_TEST_PATH)
        subprocess.run(
        f'powershell -Command "Start-Process \\"{TWITCH_TEST_PATH}\\" -Verb RunAs -WorkingDirectory \\"{test_dir}\\""',
        shell=True)
        return True
    except Exception as e:
        messagebox.showerror("錯誤", f"Twitch 測試軟體啟動失敗：{e}")
        return False
    
def launch_all_streams():
    """依勾選狀態啟動所有程式"""
    success = True

    if not launch_obs():
        success = False

    if not launch_vts():
        success = False

    if sync_onecomm.get():
        if not launch_onecomm():
            success = False

    if sync_twitch.get():
        if not launch_twitch_test():
            success = False

    if success:
        messagebox.showinfo("成功", "正在啟動所有程式！\n啟動速度會受限於電腦性能,可能需要等一下喔 !\n祝 直播順利(*´∀`)~♥")

def only_launch_twitch_test():
    """只啟動 Twitch 測試軟體"""

    launch_twitch_test()
    messagebox.showinfo("成功", "Twitch 測試軟體已成功啟動！")

# =========================
# 檢查更新
# =========================

check_update()

# =========================
# 主迴圈前置：視窗置中
# =========================

def center_window(window):
    """將視窗置中"""
    window.update_idletasks()  # 先更新視窗資訊
    width = window.winfo_width()
    height = window.winfo_height()
    screen_width = window.winfo_screenwidth()
    screen_height = window.winfo_screenheight()
    x = (screen_width // 2) - (width // 2)
    y = (screen_height // 2) - (height // 2)
    window.geometry(f'{width}x{height}+{x}+{y}')

# 初始大小
root.geometry("520x360")

# 視窗置中
center_window(root)

# =========================
# 讀取設定檔
# =========================

def load_config():
    """啟動時讀取 config.json"""

    if not os.path.exists(CONFIG_FILE):
        return

    with open(CONFIG_FILE, "r", encoding="utf-8") as f:
        data = json.load(f)

    # 設定主題
    current_theme.set(data.get("theme", "clam"))
    style.theme_use(current_theme.get())

    # 將設定值塞回 UI 變數
    obs_path.set(data.get("obs_path", ""))
    vts_path.set(data.get("vts_path", ""))
    onecomm_path.set(data.get("onecomm_path", ""))

    admin_obs.set(data.get("admin_obs", False))
    sync_onecomm.set(data.get("sync_onecomm", False))
    sync_twitch.set(data.get("sync_twitch", False))

    # 同步顯示 OneCommond
    toggle_onecomm()

# =========================
# 儲存設定檔
# =========================

def save_config():
    """關閉時儲存設定"""

    data = {
        "obs_path": obs_path.get(),
        "vts_path": vts_path.get(),
        "onecomm_path": onecomm_path.get(),
        "admin_obs": admin_obs.get(),
        "sync_onecomm": sync_onecomm.get(),
        "sync_twitch": sync_twitch.get(),
        "theme": current_theme.get(),
    }

    with open(CONFIG_FILE, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=4)

# =========================
# 選擇檔案
# =========================

def pick_file(var):
    """開啟檔案選擇視窗"""

    path = filedialog.askopenfilename()

    if path:
        var.set(path)

# =========================
# OneCommond 顯示切換
# =========================

def toggle_onecomm():
    """依勾選顯示/隱藏 OneCommond"""

    if sync_onecomm.get():
        onecomm_frame.grid()
    else:
        onecomm_frame.grid_remove()

    # 重新計算視窗大小
    root.update_idletasks()
    root.geometry("")

# =========================
# 關閉事件
# =========================

def on_close():
    """關閉視窗時呼叫"""

    save_config()
    root.destroy()

root.protocol("WM_DELETE_WINDOW", on_close)
# =========================
# 主容器
# =========================

def show_about():
    """關於 / 設定視窗"""

    win = tk.Toplevel(root)
    win.title("關於 / 設定")
    win.resizable(False, False)
    win.grab_set()  # 鎖定主視窗

    frame = ttk.Frame(win, padding=15)
    frame.pack(fill="both", expand=True)
    
    win.iconbitmap("icon/icon.ico")

    # 資訊區
    ttk.Label(
        frame,
        text=f"{APP_NAME} v0.1",
        font=("Segoe UI", 12, "bold")
    ).pack(anchor="w")

    ttk.Label(
        frame,
        text="© 2026 Joeeey0219."
    ).pack(anchor="w", pady=10)

    ttk.Label(
        frame,
        text="Twitch伺服器測試軟體 - TwitchTest by R1ch (MIT License)"
    ).pack(anchor="w", pady=(0, 10))

    ttk.Label(
        frame,
        text="軟體icon圖片 by 慕尹 Moon_yin"
    ).pack(anchor="w", pady=(0, 10))

    ttk.Separator(frame).pack(fill="x", pady=10)

    ttk.Label(
        frame,
        text="有訂製需求歡迎來電洽詢(*´∀`)~♥",
    ).pack(anchor="w", pady=(0, 10))

    ttk.Label(
        frame,
        text="聯繫方式：\nDiscord : joey0219\nGitHub : Joeeey0219",
    ).pack(anchor="w", pady=(0, 10))

    ttk.Separator(frame).pack(fill="x", pady=10)

    # 外觀設定區
    ttk.Label(
        frame,
        text="外觀主題：",
        font=("Segoe UI", 10, "bold")
    ).pack(anchor="w")

    themes = style.theme_names()

    combo = ttk.Combobox(
        frame,
        values=themes,
        textvariable=current_theme,
        state="readonly",
        width=20
    )
    combo.pack(anchor="w", pady=10)

    # 套用 & 檢查更新按鈕區
    btn_frame = ttk.Frame(frame)
    btn_frame.pack(anchor="se", pady=10, padx=10)

    def apply_theme():
        style.theme_use(current_theme.get())
        save_config()
        win.destroy()  # 套用後自動關閉設定視窗

    ttk.Button(
        btn_frame,
        text="檢查更新",
        command=check_update_GUI
    ).pack(fill="x", pady=(0,10))

    ttk.Button(
        btn_frame,
        text="套用", 
        command=apply_theme
    ).pack(fill="x")

# =========================
# 主容器
# =========================

main = ttk.Frame(root, padding=15)
main.pack(fill="both", expand=True)

# =========================
# 標題
# =========================

ttk.Label(
    main,
    text="Stream Launcher",
    font=("Segoe UI", 16, "bold")
).grid(row=0, column=0, columnspan=3, pady=(0, 20))

# =========================
# OBS
# =========================

ttk.Label(main, text="OBS").grid(row=1, column=0, sticky="w")

ttk.Entry(
    main,
    textvariable=obs_path
).grid(row=1, column=1, sticky="we", padx=5)

ttk.Button(
    main,
    text="瀏覽",
    command=lambda: pick_file(obs_path)
).grid(row=1, column=2)

# =========================
# VTS
# =========================

ttk.Label(main, text="VTube Studio").grid(row=2, column=0, sticky="w", pady=10)

ttk.Entry(
    main,
    textvariable=vts_path
).grid(row=2, column=1, sticky="we", padx=5)

ttk.Button(
    main,
    text="瀏覽",
    command=lambda: pick_file(vts_path)
).grid(row=2, column=2)

# =========================
# 勾選區（橫排）
# =========================

check_frame = ttk.Frame(main)
check_frame.grid(row=3, column=0, columnspan=3, sticky="w", pady=5)

ttk.Checkbutton(
    check_frame,
    text="管理員模式開啟 OBS",
    variable=admin_obs
).pack(side="left", padx=5)

ttk.Checkbutton(
    check_frame,
    text="同步開啟OneCommond",
    variable=sync_onecomm,
    command=toggle_onecomm
).pack(side="left", padx=5)

ttk.Checkbutton(
    check_frame,
    text="同步開啟Twitch 伺服器測試",
    variable=sync_twitch
).pack(side="left", padx=5)

# =========================
# OneCommond 區（動態）
# =========================

onecomm_frame = ttk.Frame(main)

ttk.Label(onecomm_frame, text="OneCommond").grid(row=0, column=0, sticky="w")

ttk.Entry(
    onecomm_frame,
    textvariable=onecomm_path
).grid(row=0, column=1, sticky="we", padx=5)

ttk.Button(
    onecomm_frame,
    text="瀏覽",
    command=lambda: pick_file(onecomm_path)
).grid(row=0, column=2)

onecomm_frame.grid(row=4, column=0, columnspan=3, sticky="we", pady=5)
onecomm_frame.grid_remove()

# =========================
# 按鈕區
# =========================

ttk.Separator(main).grid(
    row=5, column=0, columnspan=3,
    sticky="we", pady=10
)

ttk.Button(
    main,
    text="▶ 準備直播",
    command=launch_all_streams
).grid(row=6, column=0, columnspan=3,
       sticky="we", ipady=6, pady=5)

ttk.Button(
    main,
    text="🌐 Twitch 伺服器測試",
    command=only_launch_twitch_test
).grid(row=7, column=0, columnspan=3,
       sticky="we", ipady=6, pady=5)

ttk.Separator(main).grid(
    row=8, column=0, columnspan=3,
    sticky="we", pady=5
)

ttk.Button(
    main,
    text="ℹ 關於&設定/ About&Settings",
    command=show_about
).grid(row=9, column=0, columnspan=3,
       sticky="we", ipady=4, pady=5)

# =========================
# Grid 欄位設定
# =========================

# 中間欄位可延展
main.columnconfigure(1, weight=1)

# =========================
# 啟動時讀設定
# =========================

load_config()

# =========================
# 主迴圈
# =========================

root.mainloop()
