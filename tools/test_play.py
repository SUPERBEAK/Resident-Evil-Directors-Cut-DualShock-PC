import subprocess
import time
import os
import ctypes

exe_path = os.path.abspath('port/build/Release/re1_dualshock_port.exe')
cwd = os.path.dirname(exe_path)

# Delete existing ReShade screenshots so we only get the new ones
for f in os.listdir(cwd):
    if f.startswith('re1_dualshock_port') and f.endswith('.png'):
        try:
            os.remove(os.path.join(cwd, f))
        except:
            pass

print("Launching game in Room 1 1...")
proc = subprocess.Popen([exe_path, "--room", "1", "1"], cwd=cwd)
time.sleep(3.0)

user32 = ctypes.windll.user32
VK_W = 0x57
VK_A = 0x41
VK_S = 0x53
VK_D = 0x44
VK_F5 = 0x74
KEYEVENTF_KEYUP = 0x0002

def press_key(vk, duration):
    user32.keybd_event(vk, 0, 0, 0)
    time.sleep(duration)
    user32.keybd_event(vk, 0, KEYEVENTF_KEYUP, 0)
    time.sleep(0.3)

def take_screenshot():
    user32.keybd_event(VK_F5, 0, 0, 0)
    time.sleep(0.1)
    user32.keybd_event(VK_F5, 0, KEYEVENTF_KEYUP, 0)
    time.sleep(0.5)

print("Take initial screenshot at spawn...")
take_screenshot()

print("Walking forward towards the staircase...")
press_key(VK_W, 2.5)
take_screenshot()

print("Walking further forward right up to the stairs...")
press_key(VK_W, 2.5)
take_screenshot()

print("Walking further up...")
press_key(VK_W, 2.0)
take_screenshot()

time.sleep(1.0)
proc.terminate()

new_pngs = sorted([os.path.join(cwd, f) for f in os.listdir(cwd) if f.startswith('re1_dualshock_port') and f.endswith('.png')])
print(f"Generated {len(new_pngs)} screenshots:")
for p in new_pngs:
    print(p, os.path.getsize(p))
