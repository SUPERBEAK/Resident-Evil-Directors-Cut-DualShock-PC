import subprocess
import time
import os
import ctypes
from ctypes import wintypes
import struct

exe_path = os.path.abspath('port/build/Release/re1_dualshock_port.exe')
cwd = os.path.dirname(exe_path)

print("Launching in Main Hall: --room 1 1 ...")
proc = subprocess.Popen([exe_path, "--room", "1", "1"], cwd=cwd)
time.sleep(3.0)

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

found_hwnd = None
found_title = ""
def enum_cb(hwnd, lparam):
    global found_hwnd, found_title
    if user32.IsWindowVisible(hwnd):
        length = user32.GetWindowTextLengthW(hwnd)
        if length > 0:
            buff = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buff, length + 1)
            if "Resident Evil" in buff.value:
                found_hwnd = hwnd
                found_title = buff.value
                return False
    return True

WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
user32.EnumWindows(WNDENUMPROC(enum_cb), 0)

if not found_hwnd:
    print("Window not found")
    proc.terminate()
    exit(1)

print(f"Active Window Title in Room 1: '{found_title}'")

# Capture window
rect = wintypes.RECT()
user32.GetWindowRect(found_hwnd, ctypes.byref(rect))
w = rect.right - rect.left
h = rect.bottom - rect.top

hdc_screen = user32.GetDC(0)
hdc_dest = gdi32.CreateCompatibleDC(hdc_screen)
hbm = gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
gdi32.SelectObject(hdc_dest, hbm)
gdi32.BitBlt(hdc_dest, 0, 0, w, h, hdc_screen, rect.left, rect.top, 0x00CC0020)

class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ('biSize', wintypes.DWORD), ('biWidth', wintypes.LONG), ('biHeight', wintypes.LONG),
        ('biPlanes', wintypes.WORD), ('biBitCount', wintypes.WORD), ('biCompression', wintypes.DWORD),
        ('biSizeImage', wintypes.DWORD), ('biXPelsPerMeter', wintypes.LONG), ('biYPelsPerMeter', wintypes.LONG),
        ('biClrUsed', wintypes.DWORD), ('biClrImportant', wintypes.DWORD)
    ]

bmi = BITMAPINFOHEADER()
bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
bmi.biWidth = w
bmi.biHeight = h
bmi.biPlanes = 1
bmi.biBitCount = 32
bmi.biCompression = 0

buf = (ctypes.c_char * (w * h * 4))()
gdi32.GetDIBits(hdc_dest, hbm, 0, h, buf, ctypes.byref(bmi), 0)

out_bmp = 'tools/capture_hall_verified.bmp'
bmp_hdr = struct.pack('<2sIHHI', b'BM', 14 + 40 + w * h * 4, 0, 0, 54)
info_hdr = struct.pack('<IIIHHIIIIII', 40, w, h, 1, 32, 0, w * h * 4, 0, 0, 0, 0)
with open(out_bmp, 'wb') as f:
    f.write(bmp_hdr)
    f.write(info_hdr)
    f.write(buf)

print(f"Saved {out_bmp}")
gdi32.DeleteObject(hbm)
gdi32.DeleteDC(hdc_dest)
user32.ReleaseDC(0, hdc_screen)

proc.terminate()
print("Done!")
