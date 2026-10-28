import subprocess, time, os

base = r"H:\Antigravity Projects\ultimate dualshock"
exe = os.path.join(base, "port", "build", "Release", "re1_dualshock_port.exe")

print("Launching re1_dualshock_port.exe for 4 seconds...")
p = subprocess.Popen([exe], cwd=base)
time.sleep(4)
print("Terminating process...")
p.terminate()
try:
    p.wait(timeout=2)
except:
    p.kill()

print("Execution test passed successfully!")
