#!/usr/bin/env bash
# Đóng gói bản Linux 64-bit "cơ động": tệp chạy + thư viện Qt + plugin nằm gọn
# trong một thư mục, chạy được mà không cần cài Qt trên máy đích.
#
#   ./packaging/linux/make-portable.sh <tệp-chạy> <thư-mục-Qt> <thư-mục-đích>
#
# Ví dụ:
#   ./packaging/linux/make-portable.sh build/bin/TuanMultiTeleClient \
#       /opt/Qt/6.4.3/gcc_64 dist/TuanMultiTeleClient-linux-x64

set -euo pipefail

BINARY=${1:?Thiếu đường dẫn tệp chạy}
QT_PREFIX=${2:?Thiếu thư mục cài Qt}
OUT_DIR=${3:?Thiếu thư mục đích}

if [[ ! -x "$BINARY" ]]; then
    echo "Không tìm thấy tệp chạy: $BINARY" >&2
    exit 1
fi

QT_LIB_DIR="$QT_PREFIX/lib"
QT_PLUGIN_DIR="$QT_PREFIX/plugins"

if [[ ! -d "$QT_LIB_DIR" ]]; then
    echo "Không tìm thấy thư mục thư viện Qt: $QT_LIB_DIR" >&2
    exit 1
fi

BINARY_NAME=$(basename "$BINARY")

echo ">> Dọn thư mục đích: $OUT_DIR"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/lib/plugins"

echo ">> Copy tệp chạy"
cp "$BINARY" "$OUT_DIR/"
chmod +x "$OUT_DIR/$BINARY_NAME"

# --- Lần theo phụ thuộc bằng DT_NEEDED --------------------------------------
# Không dùng ldd vì tệp chạy đã được đặt RPATH = $ORIGIN/lib, nên lúc đóng gói
# ldd chưa tìm thấy thư viện Qt ở đâu cả. Đọc thẳng DT_NEEDED rồi tra trong thư
# mục Qt là cách chắc chắn.
needed_libs() {
    readelf -d "$1" 2>/dev/null | awk -F'[][]' '/\(NEEDED\)/ {print $2}'
}

# Copy một thư viện theo tên mà trình nạp tìm (SONAME, ví dụ libicudata.so.74).
#
# Trong thư mục Qt, tên đó thường là symlink trỏ tới tệp thật có thêm số phụ
# (libicudata.so.74.2). Nếu cứ "cp -L" theo SONAME thì được một tệp thật, rồi
# bước gom ICU bên dưới lại copy tệp thật lần nữa dưới tên đầy đủ — riêng
# libicudata đã 30 MB nên gói phình thêm ~35 MB vô ích. Vì vậy: copy tệp thật
# đúng một lần, còn SONAME chỉ là symlink.
copy_lib_by_soname() {
    local name=$1
    local src="$QT_LIB_DIR/$name"
    [[ -e "$src" ]] || return 1

    local real real_name
    real=$(readlink -f "$src")
    real_name=$(basename "$real")

    if [[ ! -f "$OUT_DIR/lib/$real_name" ]]; then
        cp -L "$real" "$OUT_DIR/lib/$real_name"
    fi
    if [[ "$real_name" != "$name" && ! -e "$OUT_DIR/lib/$name" ]]; then
        ln -sf "$real_name" "$OUT_DIR/lib/$name"
    fi
    return 0
}

copy_qt_deps() {
    local target=$1
    local name
    while IFS= read -r name; do
        [[ -n "$name" ]] || continue
        case "$name" in
            libQt6*|libicu*)
                if [[ ! -e "$OUT_DIR/lib/$name" ]] && copy_lib_by_soname "$name"; then
                    # Thư viện Qt lại cần thư viện Qt khác — đệ quy tiếp.
                    copy_qt_deps "$OUT_DIR/lib/$name"
                fi
                ;;
        esac
    done < <(needed_libs "$target")
}

echo ">> Lần theo phụ thuộc của tệp chạy"
copy_qt_deps "$OUT_DIR/$BINARY_NAME"

# ICU được Qt nạp động nên có thể không xuất hiện trong DT_NEEDED của tệp chạy.
for base in libicudata libicui18n libicuuc; do
    for candidate in "$QT_LIB_DIR/$base".so.*; do
        [[ -e "$candidate" ]] || continue
        copy_lib_by_soname "$(basename "$candidate")" || true
    done
done

# --- Copy plugin -------------------------------------------------------------
# platforms   : bắt buộc (libqxcb.so — thiếu là không mở được cửa sổ)
# imageformats: đọc ảnh jpeg/webp trong tin nhắn
# còn lại     : tuỳ môi trường desktop (Wayland, GTK theme…)
PLUGIN_GROUPS=(
    platforms
    platformthemes
    platforminputcontexts
    imageformats
    iconengines
    xcbglintegrations
    wayland-shell-integration
    wayland-graphics-integration-client
    wayland-decoration-client
    generic
    tls
)

for group in "${PLUGIN_GROUPS[@]}"; do
    src="$QT_PLUGIN_DIR/$group"
    [[ -d "$src" ]] || continue
    echo ">> Copy plugin: $group"
    mkdir -p "$OUT_DIR/lib/plugins/$group"
    cp -L "$src"/*.so "$OUT_DIR/lib/plugins/$group/" 2>/dev/null || true
    for plugin in "$OUT_DIR/lib/plugins/$group"/*.so; do
        [[ -e "$plugin" ]] || continue
        copy_qt_deps "$plugin"
        # Plugin nằm sâu hai cấp so với ./lib nên cần RPATH riêng.
        if command -v patchelf >/dev/null 2>&1; then
            patchelf --set-rpath '$ORIGIN/../..' "$plugin" 2>/dev/null || true
        fi
    done
done

# --- qt.conf để Qt biết tìm plugin ở đâu ------------------------------------
cat > "$OUT_DIR/qt.conf" <<'QTCONF'
[Paths]
Prefix = .
Libraries = lib
Plugins = lib/plugins
QTCONF

# --- Bảo đảm RPATH của tệp chạy trỏ vào ./lib -------------------------------
if command -v patchelf >/dev/null 2>&1; then
    patchelf --set-rpath '$ORIGIN/lib' "$OUT_DIR/$BINARY_NAME" 2>/dev/null || true
fi

# --- Tệp khởi chạy tiện dụng -------------------------------------------------
cat > "$OUT_DIR/chay-ung-dung.sh" <<LAUNCH
#!/usr/bin/env bash
# Khởi chạy Tuấn' MultiTele Client (bản cơ động).
# Mọi dữ liệu sẽ nằm trong thư mục "data" cạnh tệp này.
HERE="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="\$HERE/lib:\${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="\$HERE/lib/plugins"
exec "\$HERE/$BINARY_NAME" "\$@"
LAUNCH
chmod +x "$OUT_DIR/chay-ung-dung.sh"

# --- Tệp .desktop (nếu muốn tạo lối tắt trong menu) -------------------------
cat > "$OUT_DIR/tuan-multitele-client.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Tuấn' MultiTele Client
Comment=Trình khách Telegram đa tài khoản
Exec=chay-ung-dung.sh
Icon=tuan-multitele-client
Terminal=false
Categories=Network;InstantMessaging;
StartupWMClass=TuanMultiTeleClient
DESKTOP

# --- Kiểm tra nhanh: đã đủ thư viện chưa? -----------------------------------
echo ">> Thư viện đã gói:"
ls -1 "$OUT_DIR/lib"/*.so* 2>/dev/null | xargs -r -n1 basename | sort

missing=$(LD_LIBRARY_PATH="$OUT_DIR/lib" ldd "$OUT_DIR/$BINARY_NAME" 2>/dev/null \
    | awk '/not found/ {print $1}' || true)
if [[ -n "$missing" ]]; then
    echo ">> CẢNH BÁO: còn thiếu thư viện:" >&2
    echo "$missing" >&2
else
    echo ">> Không thiếu thư viện nào."
fi

echo ">> Xong. Kích cỡ gói:"
du -sh "$OUT_DIR"
