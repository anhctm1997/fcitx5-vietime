# Planning xây dựng bộ gõ tiếng Việt bằng Rust cho Ubuntu

## 1. Mục tiêu dự án

Xây dựng một bộ gõ tiếng Việt dành riêng cho Ubuntu, ưu tiên trải nghiệm ổn định trên GNOME Wayland, không xuất hiện lỗi gạch chân/composition underline khi gõ trên Chrome, Chromium, Edge, Electron, VS Code, GTK và Qt.

Tên tạm thời:

**VietIME**

Mục tiêu chính:

- Ubuntu 24.04 / 26.04+
- GNOME Wayland là nền tảng ưu tiên
- Hỗ trợ X11/XWayland
- Hỗ trợ Chrome / Chromium / Edge / Firefox
- Hỗ trợ Electron / VS Code / JetBrains
- Hỗ trợ GTK3 / GTK4
- Hỗ trợ Qt5 / Qt6
- Hỗ trợ Terminal
- Telex chuẩn
- VNI
- Unicode dựng sẵn NFC
- Không client-side preedit
- Không composition underline
- Không duplicate ký tự
- Không mất phím
- Không làm hỏng shortcut Ctrl/Alt/Super
- Có chế độ Free Typing cho lập trình viên
- Có khả năng đóng gói `.deb`

---

# 2. Nguyên tắc thiết kế quan trọng nhất

Điểm cốt lõi của VietIME:

> Không gửi preedit/composition text xuống application nếu có thể tránh được.

Nhiều ứng dụng Chromium/Electron tự render composition/preedit bằng underline.

Kiến trúc VietIME sẽ ưu tiên:

```text
deleteSurroundingText()
+
commitString()
```

thay vì:

```text
setPreedit(...)
```

Mục tiêu:

```text
User typing
    ↓
Rust Vietnamese Engine
    ↓
Replace / Commit Action
    ↓
Fcitx5
    ↓
Application
```

Application chỉ nhận text đã commit.

---

# 3. Công nghệ sử dụng

## Ngôn ngữ chính

```text
Rust
```

Rust xử lý:

- Vietnamese input engine
- Telex
- VNI
- Unicode
- Tone placement
- State machine
- Syllable parser
- Minimum text replacement
- Unit test
- Property test
- Fuzzing
- Benchmark

## Fcitx5 integration

Fcitx5 hiện có native API chủ yếu bằng C++.

Do đó kiến trúc đề xuất:

```text
Fcitx5
   ↓
C++ Thin Adapter
   ↓
C ABI
   ↓
Rust Core
```

Tỷ lệ code dự kiến:

```text
Rust: 90-95%
C++ : 5-10%
```

C++ chỉ đóng vai trò bridge giữa Fcitx5 và Rust.

---

# 4. Kiến trúc tổng thể

```text
                    Ubuntu
             X11 / Wayland / GTK / Qt
                       │
                       ▼
                    Fcitx5
                       │
                       ▼
             ┌──────────────────┐
             │ C++ thin adapter │
             │   ~300-800 LOC   │
             └────────┬─────────┘
                      │ C ABI
                      ▼
       ┌──────────────────────────────┐
       │      vietime-core Rust       │
       │                              │
       │ Keyboard Processing          │
       │ Telex / VNI                  │
       │ Vietnamese Parser            │
       │ Tone Placement               │
       │ Unicode Renderer             │
       │ State Machine                │
       │ Minimum Replacement          │
       └──────────────┬───────────────┘
                      │
                      ▼
               Replace / Commit
                      │
                      ▼
             Fcitx5 InputContext
                      │
           ┌──────────┴───────────┐
           │                      │
 deleteSurroundingText()      commitString()
           │                      │
           └──────────┬───────────┘
                      ▼
                  Application
```

---

# 5. Repository Structure

```text
fcitx5-vietime/
│
├── Cargo.toml
├── README.md
├── LICENSE
│
├── crates/
│   │
│   ├── vietime-core/
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── lib.rs
│   │       ├── engine.rs
│   │       ├── state.rs
│   │       ├── syllable.rs
│   │       ├── tone.rs
│   │       ├── unicode.rs
│   │       ├── replace.rs
│   │       │
│   │       ├── telex/
│   │       │   ├── mod.rs
│   │       │   ├── parser.rs
│   │       │   └── rules.rs
│   │       │
│   │       └── vni/
│   │           ├── mod.rs
│   │           └── rules.rs
│   │
│   └── vietime-ffi/
│       ├── Cargo.toml
│       └── src/
│           └── lib.rs
│
├── fcitx/
│   ├── vietime.cpp
│   ├── vietime.h
│   └── CMakeLists.txt
│
├── data/
│   ├── vietime-addon.conf.in
│   └── vietime.conf.in
│
├── tests/
│   ├── corpus/
│   ├── integration/
│   └── compatibility/
│
├── packaging/
│   └── debian/
│
└── tools/
    └── benchmark/
```

---

# 6. Thiết kế Rust Core

## 6.1 Engine

```rust
pub struct Engine {
    state: State,
    config: Config,
}
```

Input:

```rust
pub struct KeyInput {
    pub ch: Option<char>,
    pub key: Key,
    pub modifiers: Modifiers,
}
```

Output:

```rust
pub enum Action {
    Pass,

    Consume,

    Commit {
        text: String,
    },

    Replace {
        delete_before: usize,
        text: String,
    },

    Reset,
}
```

Main API:

```rust
impl Engine {
    pub fn process(&mut self, input: KeyInput) -> Action {
        // Vietnamese processing
    }
}
```

---

# 7. Internal State

Không nên xử lý toàn bộ bằng string replace đơn giản.

State:

```rust
pub struct CompositionState {
    pub raw: String,
    pub rendered: String,
    pub syllable: Option<Syllable>,
}
```

Input mode:

```rust
pub enum InputMode {
    Vietnamese,
    Raw,
    Disabled,
}
```

Full state:

```rust
pub struct State {
    pub raw: String,
    pub rendered: String,
    pub mode: InputMode,
}
```

---

# 8. Representation ký tự tiếng Việt

Không nên coi:

```text
ế
```

chỉ là một string.

Nên biểu diễn dạng:

```rust
pub struct VietnameseChar {
    pub base: BaseChar,
    pub modifier: Modifier,
    pub tone: Tone,
}
```

Ví dụ:

```text
ế
```

được biểu diễn:

```text
base     = e
modifier = circumflex
tone     = acute
```

Ưu điểm:

- dễ xử lý Telex
- dễ xử lý VNI
- dễ đổi dấu
- dễ normalize Unicode
- dễ test

---

# 9. Tone

```rust
pub enum Tone {
    None,
    Acute,
    Grave,
    Hook,
    Tilde,
    Dot,
}
```

Mapping Telex:

```text
s → sắc
f → huyền
r → hỏi
x → ngã
j → nặng
z → bỏ dấu
```

---

# 10. Telex Mapping

```text
aa → â
aw → ă

ee → ê

oo → ô
ow → ơ

uw → ư

dd → đ
```

Ví dụ:

```text
tieesng
→ tiếng

ddawng
→ đăng

dduwowngf
→ đường

nguyeenx
→ nguyễn

Vieetj
→ Việt
```

---

# 11. Parser âm tiết tiếng Việt

Mỗi syllable được phân tích:

```text
onset
+
glide
+
nucleus
+
coda
+
tone
```

Ví dụ:

```text
nguyễn
```

có thể biểu diễn:

```text
onset   = ng
glide   = u
nucleus = yê
coda    = n
tone    = ngã
```

Struct:

```rust
pub struct Syllable {
    pub onset: Onset,
    pub glide: Glide,
    pub nucleus: Nucleus,
    pub coda: Coda,
    pub tone: Tone,
}
```

---

# 12. Luật đặt dấu

Engine phải hỗ trợ:

- cách đặt dấu tiếng Việt hiện đại
- tùy chọn kiểu truyền thống

Ví dụ:

```text
hòa / hoà
thủy / thuỷ
```

Default:

```text
Modern Vietnamese
```

Rule engine nên deterministic.

Ví dụ:

```rust
fn tone_target(s: &Syllable) -> Option<usize> {
    match s.nucleus.pattern() {
        VowelPattern::OA => {
            // modern tone placement
        }

        VowelPattern::UY => {
            // modern tone placement
        }

        _ => {
            // default rule
        }
    }
}
```

---

# 13. Zero-preedit Architecture

Mục tiêu:

```text
NO CLIENT PREEDIT
```

Rust engine chỉ giữ composition nội bộ.

Ví dụ:

```text
raw      = tieesng
rendered = tiếng
```

Nhưng Chrome không nhận:

```text
composition text
```

Thay vào đó:

```text
Rust Engine
   ↓
Action::Replace
   ↓
C++ Adapter
   ↓
deleteSurroundingText()
   ↓
commitString()
```

---

# 14. Minimum Replacement Algorithm

Không xóa toàn bộ từ mỗi khi dấu thay đổi.

Ví dụ:

```text
old: tie
new: tiê
```

Tìm longest common prefix:

```text
ti
```

Sau đó:

```text
delete_before = 1
insert = ê
```

Pseudo Rust:

```rust
pub struct Replacement {
    pub delete_before: usize,
    pub insert: String,
}

pub fn diff(old: &str, new: &str) -> Replacement {
    // calculate longest common prefix
    // count Unicode chars
}
```

Phải tính theo Unicode character/grapheme phù hợp, không dùng byte length.

---

# 15. Fcitx5 Adapter

C++ chỉ chịu trách nhiệm:

```text
Fcitx KeyEvent
    ↓
convert
    ↓
Rust KeyInput
    ↓
vietime_process_key()
    ↓
VietimeAction
    ↓
Fcitx API
```

Ví dụ:

```cpp
switch (action.type) {
case VIETIME_ACTION_COMMIT:

    inputContext->commitString(
        action.text
    );

    break;

case VIETIME_ACTION_REPLACE:

    inputContext->deleteSurroundingText(
        -action.deleteBefore,
        action.deleteBefore
    );

    inputContext->commitString(
        action.text
    );

    break;
}
```

C++ không chứa logic Telex.

---

# 16. FFI

Rust crate:

```text
vietime-ffi
```

Expose C ABI.

Ví dụ:

```rust
#[repr(C)]
pub struct VietimeAction {
    pub action_type: u32,
    pub delete_before: u32,
    pub text_ptr: *const u8,
    pub text_len: usize,
}
```

API:

```rust
#[no_mangle]
pub extern "C" fn vietime_engine_new() -> *mut VietimeEngine;
```

```rust
#[no_mangle]
pub extern "C" fn vietime_engine_free(
    engine: *mut VietimeEngine
);
```

```rust
#[no_mangle]
pub extern "C" fn vietime_process_key(
    engine: *mut VietimeEngine,
    key: u32,
    modifiers: u32,
) -> VietimeAction;
```

---

# 17. Quy tắc Memory Safety

Core:

```rust
#![forbid(unsafe_code)]
```

Chỉ:

```text
vietime-ffi
```

được dùng `unsafe`.

Architecture:

```text
SAFE RUST

vietime-core
│
├── parser
├── Telex
├── VNI
├── Unicode
├── tone
└── state

        ↓

UNSAFE BOUNDARY

vietime-ffi

        ↓

C ABI

        ↓

C++ Fcitx Adapter
```

---

# 18. Panic Safety

Rust panic không được làm crash Fcitx5.

FFI boundary phải dùng:

```rust
std::panic::catch_unwind(|| {
    engine.process(...)
})
```

Nếu có panic:

```text
reset state
+
pass-through
+
write log
```

Không để Fcitx process chết.

---

# 19. State theo InputContext

Không dùng global mutable state.

Sai:

```rust
static mut ENGINE: Engine
```

Đúng:

```text
InputContext A
    ↓
Rust Engine A

InputContext B
    ↓
Rust Engine B
```

Ví dụ:

```text
Chrome textarea
    ↓
Engine A

VS Code
    ↓
Engine B

Terminal
    ↓
Engine C
```

---

# 20. Reset State

Reset composition khi:

```text
Space
Enter
Tab
Escape

Arrow Left
Arrow Right
Arrow Up
Arrow Down

Home
End

Mouse click

Focus change

Application switch

Ctrl+C
Ctrl+V
Ctrl+X
Ctrl+Z
```

---

# 21. Shortcut Safety

Các shortcut phải pass-through:

```text
Ctrl+C
Ctrl+V
Ctrl+X
Ctrl+Z
Ctrl+S
Ctrl+A

Ctrl+Shift+T

Alt+Tab

Super

F1-F12
```

Pseudo:

```rust
if key.ctrl || key.alt || key.super_key {
    state.reset();

    return Action::Pass;
}
```

Hotkey riêng sẽ xử lý ngoại lệ.

---

# 22. Chuyển EN / VI

Đề xuất:

```text
Super + Space
```

hoặc:

```text
Ctrl + Shift
```

Config cho phép thay đổi.

Mode:

```text
EN
VI
```

---

# 23. Free Typing Mode

Ưu tiên trải nghiệm developer.

Các từ:

```text
docker
golang
postgres
redis
traefik
nginx
json
yaml
ssh
https
struct
async
```

không được bị sửa sai.

Có hai mode:

```text
Strict Vietnamese
Free Typing
```

Default:

```text
Free Typing
```

V1 chỉ sử dụng rule-based.

Không cần AI.

---

# 24. VNI

Sau khi Telex ổn định mới thêm VNI.

Mapping cơ bản:

```text
1 → sắc
2 → huyền
3 → hỏi
4 → ngã
5 → nặng

6 → â / ê / ô
7 → ơ / ư
8 → ă
9 → đ
0 → bỏ dấu
```

VNI sử dụng cùng Vietnamese core.

Chỉ khác keyboard mapping.

---

# 25. Unicode

Output default:

```text
Unicode NFC
```

Không sử dụng legacy encoding:

```text
TCVN3
VNI Windows
```

trong phiên bản đầu.

Core phải đảm bảo:

```text
valid UTF-8
```

ở mọi thời điểm.

---

# 26. Dependencies Rust

Giữ dependencies càng ít càng tốt.

Ví dụ:

```toml
[dependencies]

unicode-normalization = "..."
thiserror = "..."
smallvec = "..."
```

Không dùng:

```text
tokio
async runtime
database
network
HTTP
```

trong input hot path.

---

# 27. Không Async

Input processing phải:

```text
synchronous
deterministic
fast
```

Không:

```rust
async fn process_key()
```

Không spawn worker cho mỗi key.

---

# 28. Performance Target

Target:

```text
process_key p50 < 10 µs

process_key p99 < 100 µs
```

RAM:

```text
< 20 MB
```

CPU idle:

```text
≈ 0%
```

Không network call trong hot path.

---

# 29. Small Buffer Optimization

Một âm tiết tiếng Việt rất ngắn.

Có thể sử dụng:

```rust
SmallVec<[VietnameseChar; 16]>
```

để tránh heap allocation.

Chỉ tối ưu sau benchmark.

Không cần:

```text
rope
piece table
gap buffer
```

---

# 30. Unit Tests

Ví dụ:

```rust
#[test]
fn test_tieng() {
    assert_eq!(
        type_keys("tieesng"),
        "tiếng"
    );
}
```

```rust
#[test]
fn test_duong() {
    assert_eq!(
        type_keys("dduwowngf"),
        "đường"
    );
}
```

```rust
#[test]
fn test_nguyen() {
    assert_eq!(
        type_keys("nguyeenx"),
        "nguyễn"
    );
}
```

---

# 31. Regression Corpus

Cần có corpus lớn.

Ví dụ:

```text
tieesng → tiếng
nguyeenx → nguyễn
dduwowngf → đường
truowngf → trường
Vieetj → Việt
ddawng → đăng
chuyeenr → chuyển
```

Mục tiêu:

```text
5.000+
```

testcase ở giai đoạn stable.

---

# 32. Property Testing

Sử dụng:

```text
proptest
```

Invariant:

```text
engine không panic
state luôn hợp lệ
output luôn UTF-8 hợp lệ
reset luôn đưa engine về trạng thái ban đầu
```

---

# 33. Fuzz Testing

Dùng:

```text
cargo-fuzz
```

Target:

```text
process_key()
parse_syllable()
apply_tone()
unicode_render()
replacement_diff()
```

Kiểm tra:

```text
không crash
không panic
không OOM
không infinite loop
không corrupt state
```

---

# 34. Benchmark

Dùng:

```text
criterion
```

Benchmark:

```text
single key
Telex transform
tone placement
syllable parse
Unicode render
replacement diff
full word
```

---

# 35. Compatibility Test Matrix

## Browser

```text
Google Chrome
Chromium
Microsoft Edge
Firefox
```

## Electron

```text
VS Code
Slack
Discord
```

## Editors

```text
GNOME Text Editor
Gedit
Kate
Sublime Text
JetBrains IDE
```

## Terminal

```text
GNOME Terminal
Ptyxis
Kitty
Alacritty
VS Code Terminal
```

## Office

```text
LibreOffice Writer
LibreOffice Calc
```

## Toolkits

```text
GTK3
GTK4

Qt5
Qt6

Electron
Chromium
```

## Display Servers

```text
GNOME Wayland
GNOME Xorg
XWayland
```

---

# 36. Test đặc biệt cho lỗi gạch chân

Milestone bắt buộc:

```text
Ubuntu 26.04
+
GNOME Wayland
+
Google Chrome
+
Telex
```

Test:

```text
Tiếng Việt có dấu
```

Yêu cầu:

```text
Không underline
Không composition underline
Không nháy chữ
Không mất ký tự
Không duplicate ký tự
```

Các control cần test:

```text
Backspace
Delete
Arrow
Home
End

Ctrl+C
Ctrl+V
Ctrl+Z
Ctrl+A

Mouse click

Tab switch
Window switch
```

---

# 37. Fallback Strategy

Không phải application nào cũng hỗ trợ surrounding text giống nhau.

Priority:

```text
1. deleteSurroundingText()
2. commitString()
```

Fallback:

```text
A. Fcitx key forwarding

B. X11 Backspace simulation

C. delayed syllable commit
```

Fallback C chỉ dùng cuối cùng vì UX không tốt.

---

# 38. Application Compatibility Profiles

Có thể tạo profile:

```text
Chrome
Firefox
Electron
GTK
Qt
Terminal
RemoteDesktop
```

Config:

```text
normal
fallback
disable-replace
raw
```

---

# 39. Blacklist

Một số application có thể disable input method mặc định:

```text
Remmina
virt-manager
Steam
VM console
remote KVM
```

Config:

```ini
[Applications]

Disable=virt-manager
Disable=remmina
Disable=steam
```

---

# 40. Config

File:

```text
~/.config/fcitx5/conf/vietime.conf
```

Ví dụ:

```ini
[General]

InputMethod=Telex
ModernTone=True
FreeTyping=True

[Behavior]

RestoreState=True
ZeroPreedit=True

[Shortcut]

Toggle=Super+Space
```

---

# 41. GUI Config

Không làm GUI ở giai đoạn đầu.

GUI chỉ triển khai khi:

```text
Telex stable
+
Chrome stable
+
Wayland stable
```

Các setting:

```text
Input method
Telex / VNI

Tone style

Free typing

Toggle hotkey

Application blacklist
```

---

# 42. Build Architecture

Rust:

```text
cargo build --release
```

Tạo:

```text
libvietime_core.a
```

C++:

```text
CMake
+
Fcitx5
+
libvietime_core.a
```

Output:

```text
vietime.so
```

Đề xuất static-link Rust core vào addon.

Architecture:

```text
Rust static library
       +
C++ Fcitx adapter
       ↓
vietime.so
```

Ưu điểm:

```text
không phụ thuộc runtime libvietime.so
dễ packaging
dễ deploy
ít lỗi library path
```

---

# 43. Packaging

Package:

```text
fcitx5-vietime_0.1.0_amd64.deb
```

Install:

```bash
sudo apt install ./fcitx5-vietime_0.1.0_amd64.deb
```

Files:

```text
/usr/lib/x86_64-linux-gnu/fcitx5/vietime.so

/usr/share/fcitx5/addon/vietime.conf

/usr/share/fcitx5/inputmethod/vietime.conf

/usr/share/icons/hicolor/...
```

---

# 44. CI Pipeline

Pipeline:

```text
cargo fmt
    ↓
cargo clippy
    ↓
cargo test
    ↓
cargo test --release
    ↓
property tests
    ↓
fuzz smoke test
    ↓
benchmark regression
    ↓
cargo build --release
    ↓
CMake build
    ↓
Fcitx addon build
    ↓
integration tests
    ↓
.deb package
```

---

# 45. Static Analysis

Rust:

```text
cargo clippy
cargo fmt
```

FFI / C++:

```text
clang-format
clang-tidy
```

Sanitizers:

```text
ASAN
UBSAN
```

---

# 46. Logging

Không spam log ở hot path.

Log levels:

```text
ERROR
WARN
INFO
DEBUG
TRACE
```

Default:

```text
WARN
```

Debug mode cho phép ghi:

```text
raw key
engine state
action
application
fallback mode
```

Không log nội dung nhạy cảm mặc định.

---

# 47. Privacy

Input method xử lý keyboard input nên privacy là requirement quan trọng.

VietIME phải:

```text
không gửi dữ liệu qua network
không telemetry mặc định
không upload text
không cloud processing
```

Engine:

```text
100% local
```

---

# 48. Security

Các yêu cầu:

```text
core forbid unsafe
FFI boundary nhỏ
panic isolation
input length limit
buffer size limit
fuzz test
ASAN/UBSAN
```

Không load arbitrary plugin trong core.

---

# 49. Roadmap

## Phase 0 — Research

Mục tiêu:

```text
Fcitx5 architecture
InputContext
KeyEvent
commitString
deleteSurroundingText
Wayland behavior
Chrome behavior
```

Deliverable:

```text
technical spike
```

---

## Phase 1 — Rust Core Prototype

Implement:

```text
Engine
State
Action
Unicode
Telex basic
```

Test:

```text
tieesng
nguyeenx
dduwowngf
```

Deliverable:

```text
vietime-core
```

---

## Phase 2 — FFI

Implement:

```text
vietime-ffi
C ABI
memory ownership
panic boundary
```

Deliverable:

```text
libvietime_core.a
vietime_ffi.h
```

---

## Phase 3 — Fcitx5 Bridge

Implement:

```text
Fcitx addon
KeyEvent conversion
commitString
deleteSurroundingText
```

Deliverable:

```text
vietime.so
```

---

## Phase 4 — Zero Preedit

Mục tiêu:

```text
Không gửi client preedit
```

Test:

```text
Chrome
GNOME Wayland
```

Deliverable:

```text
zero underline prototype
```

---

## Phase 5 — Telex Complete

Implement:

```text
full Vietnamese syllable parser
tone placement
restore tone
free typing
modern tone
```

Deliverable:

```text
Telex stable
```

---

## Phase 6 — Compatibility

Test:

```text
Chrome
Firefox
VS Code
GTK
Qt
Terminal
LibreOffice
```

Deliverable:

```text
compatibility matrix
```

---

## Phase 7 — VNI

Implement:

```text
VNI mapping
reuse Vietnamese core
```

Deliverable:

```text
Telex + VNI
```

---

## Phase 8 — Configuration

Implement:

```text
config file
hotkey
Free Typing
tone style
blacklist
```

Deliverable:

```text
user configurable input method
```

---

## Phase 9 — Debian Packaging

Implement:

```text
debian/
control
rules
postinst
```

Deliverable:

```text
fcitx5-vietime_1.0.0_amd64.deb
```

---

## Phase 10 — Stable Release

Criteria:

```text
no known crash
no lost key
no duplicate key
zero underline on supported Chrome configuration
stable on GNOME Wayland
```

Release:

```text
v1.0.0
```

---

# 50. Version Planning

## v0.1.0

Scope:

```text
Ubuntu 26.04
GNOME Wayland
Fcitx5
Rust Core
Telex
Chrome
Zero Preedit
```

Không:

```text
VNI
GUI
dictionary
AI
autocorrect
```

---

## v0.2.0

```text
Firefox
VS Code
Electron
GTK3/4
Terminal
```

---

## v0.3.0

```text
Qt5
Qt6
LibreOffice
JetBrains
```

---

## v0.4.0

```text
VNI
Config
Application Profiles
```

---

## v0.5.0

```text
GUI Settings
Better Free Typing
Advanced Vietnamese rules
```

---

## v1.0.0

Criteria:

```text
Telex stable
VNI stable
Wayland stable
X11 stable

Chrome supported
Firefox supported
VSCode supported
GTK supported
Qt supported

No major known underline bug
No known data-loss bug
```

---

# 51. Definition of Done cho v0.1

Test machine:

```text
Ubuntu 26.04
GNOME Wayland
Fcitx5
Google Chrome
```

User gõ:

```text
Tiếng Việt có dấu
```

Expected:

```text
Tiếng Việt có dấu
```

Không được xuất hiện:

```text
underline
composition underline
duplicate character
missing character
visual flicker nghiêm trọng
```

Test thêm:

```text
Backspace
Arrow
Home
End

Ctrl+C
Ctrl+V
Ctrl+A
Ctrl+Z

Alt+Tab

Mouse click
```

Tất cả phải hoạt động đúng.

---

# 52. Nguyên tắc phát triển

Ưu tiên:

```text
Correctness
    ↓
Stability
    ↓
Compatibility
    ↓
Performance
    ↓
Features
```

Không ưu tiên GUI trước core.

Không thêm AI trước khi input engine ổn định.

Không thêm dictionary lớn vào hot path.

Không phụ thuộc network.

Không sử dụng preedit chỉ để có UX đẹp nếu nó làm phát sinh underline.

---

# 53. Kiến trúc cuối cùng

```text
┌──────────────────────────────────────────┐
│                  Ubuntu                  │
│                                          │
│        Wayland / X11 / GTK / Qt          │
└────────────────────┬─────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────┐
│                 Fcitx5                   │
└────────────────────┬─────────────────────┘
                     │
                     ▼
             C++ Thin Adapter
                     │
                  C ABI
                     │
                     ▼
┌──────────────────────────────────────────┐
│              VietIME Core                │
│                  Rust                    │
│                                          │
│ Key Processor                            │
│      ↓                                   │
│ Telex / VNI                              │
│      ↓                                   │
│ Vietnamese Syllable Parser               │
│      ↓                                   │
│ Tone Placement                           │
│      ↓                                   │
│ Unicode Renderer                         │
│      ↓                                   │
│ Minimum Replace                          │
│      ↓                                   │
│ Action                                   │
└────────────────────┬─────────────────────┘
                     │
                     ▼
               Fcitx Adapter
                     │
          ┌──────────┴───────────┐
          │                      │
 deleteSurroundingText()     commitString()
          │                      │
          └──────────┬───────────┘
                     │
                     ▼
                 Application

             NO CLIENT PREEDIT

                     ↓

        NO COMPOSITION UNDERLINE
```

---

# 54. Kết luận

Kiến trúc đề xuất cho VietIME:

```text
Rust Core
+
C++ Fcitx5 Thin Adapter
```

Rust là ngôn ngữ chính của project.

Rust chịu trách nhiệm cho toàn bộ logic tiếng Việt, Unicode, parser, Telex, VNI, state machine, testing và fuzzing.

C++ chỉ chịu trách nhiệm kết nối với Fcitx5.

Mục tiêu kỹ thuật đầu tiên không phải xây một bộ gõ đầy đủ tính năng.

Milestone đầu tiên phải là:

> Ubuntu 26.04 + GNOME Wayland + Chrome + Telex + không gạch chân + không mất phím.

Chỉ sau khi milestone này đạt ổn định mới tiếp tục mở rộng VNI, GUI, application profiles và release v1.0.
