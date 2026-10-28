import subprocess
import time
import os
import ctypes
from ctypes import wintypes
import struct

exe_path = os.path.abspath('port/build/Release/re1_dualshock_port.exe')
cwd = os.path.dirname(exe_path)

print("Starting test_full_walk...")
# Launch in Dining Room
proc = subprocess.Popen([exe_path, "--room", "1", "0", "--pos", "3100", "0", "1200", "0.0"], cwd=cwd)
time.sleep(2.5)

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

found_hwnd = None
def enum_cb(hwnd, lparam):
    global found_hwnd
    if user32.IsWindowVisible(hwnd):
        length = user32.GetWindowTextLengthW(hwnd)
        if length > 0:
            buff = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buff, length + 1)
            if "Resident Evil" in buff.value:
                found_hwnd = hwnd
                return False
    return True

WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
user32.EnumWindows(WNDENUMPROC(enum_cb), 0)

if not found_hwnd:
    print("Error: Window not found!")
    proc.terminate()
    exit(1)

def get_title():
    length = user32.GetWindowTextLengthW(found_hwnd)
    buff = ctypes.create_unicode_buffer(length + 1)
    user32.GetWindowTextW(found_hwnd, buff, length + 1)
    return buff.value

def send_key(vk, duration=0.1):
    KEYEVENTF_KEYUP = 0x0002
    user32.keybd_event(vk, 0, 0, 0)
    time.sleep(duration)
    user32.keybd_event(vk, 0, KEYEVENTF_KEYUP, 0)

VK_W = 0x57
VK_S = 0x53
VK_A = 0x41
VK_D = 0x44
VK_E = 0x45

print("Initial title:", get_title())

# In Dining room, Chris at (3100, 1200) facing 0.0 (+Z).
# Door 0 is at (2700, 500) facing -Z.
# Turn around towards -Z and walk to door
print("Turning towards -Z and walking to Main Hall door...")
send_key(VK_S, 1.2) # walk backward or turn
time.sleep(0.2)
print("Pressing 'E' on door...")
send_key(VK_E, 0.1)
time.sleep(1.5)
print("Title after door 1:", get_title())

proc.terminate()
