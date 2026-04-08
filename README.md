# MacroTool

X11 keyboard macro tool. Simulates keypresses to type predefined command strings without clipboard or shell execution. Triggered by a global hotkey.

---

## Dependencies

**Kali/Debian/Ubuntu:**
```bash
sudo apt-get install build-essential libx11-dev libxtst-dev dmenu
```
**Arch:**
```bash
sudo pacman -S gcc libx11 libxtst dmenu
```

---

## Build & Run

```bash
./build.sh
./MacroTool
```

Requires X11 (`$DISPLAY` must be set). Does not work on Wayland.

---

## Usage

Press **Ctrl+M** to open the macro selector (dmenu). Type to filter, Enter to execute.

### Adding macros on the fly

In the dmenu prompt, type `!add` with parameters:

```
!add /category:recon /name:nmap /command:nmap -sS -O target
!add /c:web /n:sqlmap /cmd:sqlmap -u http://target --dbs
!add /n:whoami /cmd:whoami && id
```

| Param | Short | Description |
|---|---|---|
| `/category:` | `/c:` | Category path, supports nesting with `/` |
| `/name:` | `/n:` | Macro name (required) |
| `/command:` | `/cmd:` | Command string to type (required) |

New macros are saved to `json/user-defined.json` immediately.

---

## JSON Schema

Place `.json` files in the `json/` subdirectory next to the binary. All files are loaded at selection time. Arbitrary nesting is supported — leaf string values are the macro commands.

```json
{
  "category": {
    "subcategory": {
      "macro_name": "command to type"
    },
    "flat_macro": "another command"
  }
}
```

**Display format:**
```
filename.json  | category/subcategory  | macro_name  | command to type
```

---

## Configuration

Edit the top of `src/MacroTool.c` to change the hotkey:

```c
#define HOTKEY_KEY       "m"                    // X keysym name
#define HOTKEY_MODIFIERS (ControlMask)          // ControlMask, Mod1Mask (Alt), ShiftMask
```

**Keyboard layout:** Automatically detects active XKB layout. DE (QWERTZ) has full special character mappings. To add other layouts, extend the `de_keys[]` array.
