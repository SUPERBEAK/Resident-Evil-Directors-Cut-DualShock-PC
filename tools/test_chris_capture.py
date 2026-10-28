import subprocess
import time
import os
import ctypes
from ctypes import wintypes
from PIL import Image, ImageGrab

# Launch the game executable
exe_path = os.path.abspath('port/build/Release/re1_dualshock_port.exe')
cwd = os.path.dirname(exe_path)

proc = subprocess.Popen([exe_path], cwd=cwd)
print(f"Launched PID {proc.pid}")

# Wait for game to initialize and render
time.sleep(3.0)

try:
    # Find window by title prefix
    user32 = ctypes.windll.user32
    found_hwnd = None

    def enum_cb(hwnd, lparam):
        global found_hwnd
        if user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            buff = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buff, length + 1)
            if "Resident Evil" in buff.value:
                found_hwnd = hwnd
                return False
        return True

    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows(WNDENUMPROC(enum_cb), 0)

    if found_hwnd:
        print(f"Found game window HWND: {found_hwnd}")
        rect = wintypes.RECT()
        user32.GetWindowRect(found_hwnd, ctypes.byref(rect))
        bbox = (rect.left, rect.top, rect.right, rect.bottom)
        print(f"Window bounds: {bbox}")
        
        # Take screenshot of window
        img = ImageGrab.grab(bbox)
        img.save("tools/game_capture.png")
        print("Saved tools/game_capture.png")
        
        # Crop Chris in the center
        w, h = img.size
        # Chris is typically near center of screen
        crop_box = (int(w * 0.25), int(h * 0.15), int(w * 0.75), int(h * 0.90))
        chris_crop = img.crop(crop_box)
        chris_crop.save("tools/chris_crop.png")
        print("Saved tools/chris_crop.png")
    else:
        print("Window not found, taking full desktop screenshot...")
        img = ImageGrab.grab()
        img.save("tools/game_capture.png")
finally:
    proc.terminate()
    try:
        proc.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        proc.kill()
    print("Process terminated.")
