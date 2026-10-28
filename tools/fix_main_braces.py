with open("port/src/main.cpp", "r", encoding="utf-8") as f:
    text = f.read()

# Fix the extra closing brace before `} else { \n // Room Scenery Examination`
old_err = """                    }
                  });
              }
            }
          } else {
            // Room Scenery Examination"""

new_err = """                    }
                  });
              }
            }
          }
        } else {
          // Room Scenery Examination"""

text = text.replace(old_err.replace("\r\n", "\n"), new_err.replace("\r\n", "\n"))

# Normalize line endings to Windows CRLF for all files
text = text.replace("\r\n", "\n").replace("\n", "\r\n")

with open("port/src/main.cpp", "w", encoding="utf-8") as f:
    f.write(text)

# Also normalize room_manager.h and room_manager.cpp
for p in ["port/include/room_manager.h", "port/src/room_manager.cpp"]:
    with open(p, "r", encoding="utf-8") as f:
        c = f.read()
    c = c.replace("\r\n", "\n").replace("\n", "\r\n")
    with open(p, "w", encoding="utf-8") as f:
        f.write(c)

print("Bracing and line endings normalized successfully.")
