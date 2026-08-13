# VietIME

VietIME là bộ gõ tiếng Việt kiểu Telex dành cho Fcitx5 trên Linux. Bộ xử lý
Telex và Unicode được viết bằng Rust; addon Fcitx5 và giao diện cấu hình ứng
dụng được viết bằng C++/Qt 6.

VietIME chọn cơ chế nhập theo từng ứng dụng:

- ứng dụng hỗ trợ surrounding text được sửa trực tiếp tại con trỏ;
- terminal dùng buffer nội bộ và chỉ commit khi kết thúc từ để tránh gửi
  Backspace gây nhấp nháy;
- ứng dụng nằm trong danh sách chống nhấp nháy dùng client/server preedit theo
  capability mà Fcitx5 cung cấp.

Trên GNOME Wayland/X11, extension đi kèm có thể cung cấp desktop ID, app ID,
WM class và executable của cửa sổ đang focus. Nhờ đó VietIME nhận diện chính
xác hơn các ứng dụng như GoLand và Visual Studio Code khi Fcitx5 chỉ báo
`gnome-shell`.

## Tính năng

- Telex, Unicode NFC và cách đặt dấu tiếng Việt hiện đại.
- Hỗ trợ nhập dấu và biến đổi nguyên âm ở cuối âm tiết.
- State độc lập cho từng Fcitx5 `InputContext`.
- Cơ chế riêng cho terminal nhằm hạn chế nhấp nháy và ký tự lặp.
- Danh sách ứng dụng chống nhấp nháy có thể cấu hình bằng GUI.
- Thêm rule từ ứng dụng đang mở, ứng dụng gần đây hoặc nhập thủ công.
- Rule theo program/application ID và capability của input context.
- GNOME Shell focus tracker tùy chọn cho Wayland và X11.
- Rust dependencies được vendored để build offline và tái lập được.
- Gói Debian/Ubuntu và GitHub Actions cho CI/release.

VietIME hiện chỉ hỗ trợ Telex. VNI chưa được triển khai.

## Ví dụ Telex

| Phím gõ | Kết quả |
|---|---|
| `tieesng` | `tiếng` |
| `nguyeenx` | `nguyễn` |
| `dduwowngf` | `đường` |
| `Vieetj` | `Việt` |
| `suawr` | `sửa` |

## Nền tảng hỗ trợ

Quy trình CI hiện build và kiểm tra gói `amd64` trên:

- Ubuntu 24.04 (Noble);
- Ubuntu 26.04 (Resolute).

Hãy dùng đúng gói `.deb` cho phiên bản Ubuntu tương ứng. Các bản phân phối
khác có Fcitx5 và Qt 6 có thể build từ mã nguồn, nhưng hiện chưa nằm trong ma
trận kiểm thử chính thức.

## Cài nhanh từ GitHub Release

Tải file `.deb` đúng với phiên bản Ubuntu và file `SHA256SUMS` từ trang
[GitHub Releases](https://github.com/anhctm1997/fcitx5-vietime/releases).
Trong thư mục đã tải xuống, kiểm tra checksum rồi cài đặt:

```bash
package=./fcitx5-vietime_<version>_amd64.deb
grep " $(basename "$package")$" SHA256SUMS | sha256sum --check
sudo apt install "$package"
```

Khởi động lại Fcitx5 trong phiên desktop hiện tại:

```bash
fcitx5-remote -r
```

Nếu lệnh trên không nạp lại addon, đăng xuất rồi đăng nhập lại.

Mở **Fcitx5 Configuration** → **Add Input Method**, bỏ chọn
**Only Show Current Language**, tìm **VietIME** và thêm vào danh sách.

Nếu hệ thống chưa dùng Fcitx5:

```bash
sudo apt install fcitx5 im-config
im-config -n fcitx5
```

Sau đó đăng xuất và đăng nhập lại.

## Build từ mã nguồn

### 1. Cài phụ thuộc

Trên Ubuntu 24.04/26.04:

```bash
sudo apt update
sudo apt install \
  cargo \
  cmake \
  g++ \
  libfcitx5core-dev \
  libfcitx5-qt6-dev \
  pkg-config \
  qt6-base-dev \
  rustc
```

Để chạy thêm `rustfmt` và `clippy`, có thể dùng Rust từ `rustup` hoặc cài các
component tương ứng do bản phân phối cung cấp. Repository đã chứa source của
các Rust crate trong `vendor/`; `.cargo/config.toml` buộc Cargo hoạt động
offline.

### 2. Build và kiểm thử Rust

```bash
cargo fmt --all -- --check
cargo clippy --workspace --all-targets --locked --offline -- -D warnings
cargo test --workspace --release --locked --offline
cargo build --release --locked --offline -p vietime-ffi
```

Thư viện tĩnh dùng bởi addon được tạo tại
`target/release/libvietime_ffi.a`.

### 3. Build addon Fcitx5 và GUI cấu hình

```bash
multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"

cmake -S fcitx -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_LIBDIR="lib/$multiarch" \
  -DVIETIME_RUST_LIBRARY="$PWD/target/release/libvietime_ffi.a"

cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure
```

Nếu chỉ cần addon, không cần GUI cấu hình Qt:

```bash
cmake -S fcitx -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DVIETIME_BUILD_QT_EDITOR=OFF \
  -DCMAKE_INSTALL_LIBDIR="lib/$multiarch" \
  -DVIETIME_RUST_LIBRARY="$PWD/target/release/libvietime_ffi.a"
```

### 4. Cài bản build từ mã nguồn

```bash
sudo cmake --install build/release
fcitx5-remote -r
```

Lệnh này cài addon, metadata input method, GUI cấu hình và GNOME Shell
extension vào `/usr`. Cài bằng gói `.deb` được khuyến nghị vì có thể nâng cấp
và gỡ bỏ sạch bằng trình quản lý gói.

## Tạo gói `.deb`

### Phụ thuộc đóng gói

```bash
sudo apt update
sudo apt install \
  cargo \
  cmake \
  debhelper \
  devscripts \
  g++ \
  libfcitx5core-dev \
  libfcitx5-qt6-dev \
  lintian \
  pkg-config \
  qt6-base-dev \
  rustc
```

Kiểm tra các build dependency đã đầy đủ:

```bash
dpkg-checkbuilddeps
```

### Build package

Từ thư mục gốc repository:

```bash
dpkg-buildpackage -b -us -uc
```

`dpkg-buildpackage` chạy Cargo ở chế độ `--locked --offline`, chạy unit test
Rust và CTest, sau đó tạo package ở thư mục cha của repository:

```text
../fcitx5-vietime_<version>_<architecture>.deb
../fcitx5-vietime-dbgsym_<version>_<architecture>.ddeb
```

Kiểm tra package chính:

```bash
package="$(find .. -maxdepth 1 -name 'fcitx5-vietime_*_amd64.deb' -print -quit)"
lintian --fail-on error "$package"
scripts/verify-deb.sh "$package"
dpkg-deb --info "$package"
dpkg-deb --contents "$package"
```

Cài thử package vừa build:

```bash
sudo apt install "$package"
fcitx5-remote -r
```

### Build cho đúng Ubuntu codename

Script release hỗ trợ `noble` và `resolute`. Nó kiểm tra phiên bản trong
`Cargo.toml`, metadata addon và Git tag trước khi thêm Debian changelog entry:

```bash
export DEBFULLNAME="VietIME Contributors"
export DEBEMAIL="anhcuongdev@gmail.com"
scripts/package-version.sh noble    # Ubuntu 24.04
# hoặc: scripts/package-version.sh resolute
dpkg-buildpackage -b -us -uc
```

Không chạy lần lượt cả hai codename trong cùng một working tree. Mỗi distro
nên được build trong checkout/container/runner riêng, giống GitHub Actions.

## Cấu hình chống nhấp nháy theo ứng dụng

Trong **Fcitx5 Configuration**, chọn **VietIME** → **Configure**. Phần
**Applications using anti-flicker preedit** cho phép:

- chọn ứng dụng đang mở;
- chọn ứng dụng được phát hiện gần đây;
- nhập program/application ID thủ công;
- khớp wildcard `*` và `?`;
- thêm capability như `Terminal`, `Preedit`, `SurroundingText`, `Multiline`
  hoặc `ClientSideInputPanel`.

Rule không phân biệt chữ hoa/thường và được lưu tại:

```text
~/.config/fcitx5/conf/vietime-applications.conf
```

Thay đổi được áp dụng ngay. VietIME commit từ đang soạn trước khi chuyển cơ
chế nhập.

### Bật nhận diện ứng dụng trên GNOME

Gói VietIME cài extension hệ thống `vietime@vietime.invalid`. Sau lần cài đầu
tiên, đăng xuất/đăng nhập để GNOME Shell quét extension, sau đó chạy:

```bash
gnome-extensions enable vietime@vietime.invalid
gnome-extensions info vietime@vietime.invalid
```

Nếu chưa có công cụ quản lý extension:

```bash
sudo apt install gnome-shell-extensions-common
```

Extension chỉ xuất metadata cửa sổ đang focus qua session D-Bus. Nó không tự
bật khi cài package. Nếu không dùng GNOME hoặc extension chưa chạy, VietIME vẫn
dùng `program` và capability do Fcitx5 cung cấp.

## Nâng cấp và gỡ cài đặt

Nâng cấp giữ nguyên cấu hình người dùng:

```bash
sudo apt install ./fcitx5-vietime_<new-version>_amd64.deb
fcitx5-remote -r
```

Gỡ package:

```bash
sudo apt remove fcitx5-vietime
```

Muốn xóa cả rule riêng của VietIME:

```bash
rm ~/.config/fcitx5/conf/vietime-applications.conf
```

Lệnh cuối chỉ xóa cấu hình VietIME của người dùng hiện tại; package manager
không tự xóa file trong thư mục home.

## Xử lý sự cố

### Không thấy VietIME trong Fcitx5 Configuration

```bash
fcitx5-remote -r
fcitx5-diagnose
```

Xác nhận các file đã được cài:

```bash
multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
test -f "/usr/lib/$multiarch/fcitx5/vietime.so"
test -f /usr/share/fcitx5/addon/vietime.conf
test -f /usr/share/fcitx5/inputmethod/vietime.conf
```

### Extension GNOME “không tồn tại”

Xác nhận package chứa extension:

```bash
test -f /usr/share/gnome-shell/extensions/vietime@vietime.invalid/metadata.json
```

Sau đó đăng xuất và đăng nhập lại. Trên Wayland, restart Fcitx5 không làm GNOME
Shell quét lại extension mới cài.

### Danh sách ứng dụng đang mở bị trống

Kiểm tra extension đã bật và session D-Bus có service VietIME. Nếu extension
không dùng được, chuyển sang tab nhập thủ công và lấy program ID từ thông tin
chẩn đoán trong cửa sổ cấu hình hoặc `fcitx5-diagnose`.

Lưu ý: đầu ra `fcitx5-diagnose` có thể chứa thông tin hệ thống và tên ứng dụng;
hãy kiểm tra trước khi đăng công khai.

### Đã thêm ứng dụng nhưng vẫn nhấp nháy

Mở lại phần cấu hình VietIME và kiểm tra trạng thái focus, identity thực tế,
rule đã khớp và engine mode. Với GNOME, bảo đảm focus tracker đang hoạt động.
Tên process không phải lúc nào cũng giống desktop ID hoặc application ID, vì
vậy nên thêm ứng dụng từ danh sách đang mở khi có thể.

## Chuẩn bị phát hành trên GitHub

Trước khi public repository:

1. Chọn UUID/domain ổn định cho GNOME extension. Đổi
   `vietime@vietime.invalid` sau khi đã có người dùng sẽ làm GNOME coi đó là
   extension khác.
2. Kiểm tra `LICENSE`, thông tin copyright và license của thư mục `vendor/`.
3. Chạy toàn bộ kiểm thử và build package trên từng Ubuntu được hỗ trợ.
4. Không commit `target/`, `build/`, `obj-*`, `dist/` hoặc output trong
   `debian/`; hãy kiểm tra `git status` trước khi push.

### Checklist tạo release

1. Cập nhật cùng một phiên bản trong `Cargo.toml`, `Cargo.lock` và
   `data/vietime-addon.conf.in`.
2. Thêm entry mới vào `debian/changelog`.
3. Chạy:

   ```bash
   cargo fmt --all -- --check
   cargo clippy --workspace --all-targets --locked --offline -- -D warnings
   cargo test --workspace --release --locked --offline
   ```

4. Commit thay đổi, tạo tag trùng phiên bản workspace và push:

   ```bash
   git tag -a v0.1.24 -m "VietIME 0.1.24"
   git push origin main
   git push origin v0.1.24
   ```

Tag dạng `v*.*.*` kích hoạt `.github/workflows/release.yml`. Workflow build
riêng trên Ubuntu 24.04 và 26.04, chạy test/lint/kiểm tra package, cài-gỡ thử,
tạo `SHA256SUMS` và xuất bản GitHub Release.

## Cấu trúc repository

```text
crates/vietime-core/       Telex engine và state machine Rust
crates/vietime-ffi/        C ABI dùng bởi addon Fcitx5
fcitx/                     addon C++, config và Qt editor
gnome-extension/           GNOME Shell focus tracker
data/                      metadata addon/input method
debian/                    Debian packaging
scripts/                   script versioning và kiểm tra package
vendor/                    Rust dependencies dùng cho offline build
.github/workflows/         CI và quy trình GitHub Release
```

## Đóng góp

Khi báo lỗi nhập liệu, vui lòng cung cấp:

- chuỗi phím đã gõ và kết quả mong đợi/thực tế;
- ứng dụng, desktop environment và Wayland/X11;
- phiên bản Ubuntu, Fcitx5 và VietIME;
- ứng dụng có nằm trong danh sách anti-flicker hay không.

Mỗi sửa lỗi Telex nên có regression test trong `vietime-core`. Mỗi thay đổi
liên quan nhận diện ứng dụng hoặc package nên được kiểm tra cả unit test lẫn
desktop runtime; build thành công không đảm bảo mọi toolkit hỗ trợ surrounding
text giống nhau.

## Giấy phép

VietIME được phát hành theo giấy phép MIT. Xem [LICENSE](LICENSE).
