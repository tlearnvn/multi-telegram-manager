# Hướng dẫn dùng Tuấn' MultiTele Client

Tài liệu này đi kèm ngay trong gói tải về. Đọc một lượt là dùng được.

---

## 1. Chạy lần đầu

### Windows

1. Giải nén tệp `TuanMultiTeleClient-…-win-x64.zip` vào thư mục **có quyền
   ghi** — ví dụ `D:\MultiTele`, `Desktop`, hoặc thẳng vào USB.
   *Đừng* để trong `C:\Program Files`: ở đó Windows không cho ghi nên dữ liệu
   sẽ bị đẩy sang thư mục người dùng, mất tính cơ động.
2. Chạy `TuanMultiTeleClient.exe`.
3. Nếu Windows Defender hỏi, chọn *More info → Run anyway* (bản build không có
   chứng thư số).

### Debian / Ubuntu

```bash
tar -xzf TuanMultiTeleClient-*-linux-x64.tar.gz
cd TuanMultiTeleClient-*-linux-x64
./chay-ung-dung.sh
```

Nếu báo thiếu thư viện hệ thống:

```bash
sudo apt install libxcb-xinerama0 libxcb-icccm4 libxcb-image0 \
     libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 \
     libxcb-shape0 libxcb-xkb1 libxkbcommon-x11-0
```

---

## 2. Khai báo `api_id` / `api_hash`

Telegram yêu cầu **mỗi ứng dụng** phải có khoá riêng — kể cả ứng dụng bạn chỉ
dùng cho mình. Lấy khoá mất khoảng một phút và chỉ làm **một lần**:

1. Mở <https://my.telegram.org> trên trình duyệt.
2. Đăng nhập bằng **số điện thoại Telegram** của bạn (Telegram sẽ gửi mã vào
   ứng dụng trên điện thoại, không phải SMS).
3. Bấm **API development tools**.
4. Điền form:
   - *App title*: `MultiTele` (hay tên gì cũng được)
   - *Short name*: `multitele`
   - *Platform*: chọn **Desktop**
   - *Description*: để trống được
5. Bấm **Create application**.
6. Sao chép **App api_id** (dãy số) và **App api_hash** (32 ký tự) vào trình
   hướng dẫn của ứng dụng.

> Khoá được lưu ở `data/config.ini`. **Không chia sẻ `api_hash`** — người khác
> dùng khoá của bạn để spam thì khoá bị Telegram vô hiệu.

Sau này muốn đổi khoá: **Cài đặt → Nâng cao → Khoá API Telegram**.

---

## 3. Thư viện TDLib

TDLib là phần lo toàn bộ việc nói chuyện với máy chủ Telegram (mã hoá MTProto,
đồng bộ tin nhắn…). Ứng dụng nạp nó **lúc chạy**, nên chỉ cần tệp thư viện nằm
đúng chỗ:

| Hệ điều hành | Tên tệp | Đặt ở đâu |
|---|---|---|
| Windows | `tdjson.dll` | cạnh `TuanMultiTeleClient.exe`, hoặc trong `lib\` |
| Linux | `libtdjson.so` | cạnh tệp chạy, hoặc trong `lib/` |

Các bản tải về từ Releases **đã đóng kèm sẵn** — bạn không phải làm gì. Nếu
thiếu (tệp `THIEU-TDLIB.txt` xuất hiện trong gói) thì:

- tự biên dịch từ <https://github.com/tdlib/td> (xem README của dự án), hoặc
- vào **Cài đặt → Nâng cao → Thư viện TDLib → Chọn tệp…** để trỏ tới tệp
  `tdjson` có sẵn ở nơi khác trên máy.

Cần TDLib **1.8.0 trở lên**.

---

## 4. Thêm tài khoản

Bấm dấu **+** ở thanh dọc bên trái (hoặc `Ctrl+Shift+N`).

### Cách nhanh nhất: quét mã QR

1. Chọn **Quét mã QR bằng điện thoại**.
2. Trên điện thoại: mở **Telegram → Cài đặt → Thiết bị → Liên kết thiết bị
   máy tính**.
3. Quét mã hiện trên màn hình. Xong.

Mã QR tự làm mới khi hết hạn. Nếu điện thoại không quét được, bấm biểu tượng
sao chép để lấy liên kết `tg://login?...` rồi mở liên kết đó trên điện thoại.

### Cách dùng số điện thoại

1. Chọn **Dùng số điện thoại**.
2. Nhập số kèm mã quốc gia — Việt Nam là `+84`, ví dụ `+84912345678`.
3. Nhập mã xác thực Telegram gửi tới. Ứng dụng tự gửi khi bạn nhập đủ số.
4. Nếu tài khoản bật **xác thực hai lớp**, nhập thêm mật khẩu đám mây.

**Lặp lại để thêm tài khoản thứ hai, thứ ba…** Không có giới hạn số tài khoản.
Tất cả chạy song song và đều nhận tin nhắn cùng lúc.

---

## 5. Làm việc với nhiều tài khoản

**Thanh dọc bên trái** là trung tâm điều khiển:

- Mỗi avatar là một tài khoản. Vòng màu = tài khoản đang xem.
- Huy hiệu số = số cuộc trò chuyện chưa đọc của tài khoản đó.
- Đốm vàng/đỏ ở góc trên avatar = chưa đăng nhập xong hoặc có lỗi. Đưa chuột
  vào để xem chi tiết.
- **Bấm phải** vào avatar để: đổi tên hiển thị, đổi màu nhấn, mở lại kết nối,
  đăng nhập, đăng xuất, xoá tài khoản.

**Chuyển tài khoản**: bấm avatar, hoặc `Ctrl+1` … `Ctrl+9`, hoặc `Ctrl+Tab`.

**Bảng điều khiển** (`Ctrl+D` hoặc biểu tượng ô vuông ở thanh dọc) hiện bảng
tổng quan: trạng thái, số cuộc trò chuyện, tin chưa đọc và **dung lượng đĩa**
của từng tài khoản. Bấm phải vào một dòng để dọn bộ đệm, mở thư mục dữ liệu,
đăng xuất hoặc xoá.

---

## 6. Gửi tin hàng loạt

**Menu ☰ trên danh sách chat → Gửi tin hàng loạt** (hoặc menu *Tài khoản*).

1. Soạn nội dung (dùng được Markdown: `**đậm**`, `__nghiêng__`, `` `mã` ``).
2. Bấm **Kèm tệp…** nếu muốn gửi kèm ảnh hoặc tài liệu.
3. Bấm **Chọn cuộc trò chuyện…** — danh sách gộp **mọi tài khoản** đang đăng
   nhập, tích chọn bao nhiêu nơi cũng được.
4. Đặt **giãn cách** giữa hai lần gửi (mặc định 3 giây).
5. Bấm **Bắt đầu gửi**. Nhật ký bên dưới báo từng nơi thành công hay lỗi; bấm
   **Dừng** để ngắt giữa chừng.

> ⚠️ **Cẩn thận:** gửi quá nhanh hoặc gửi cho người chưa đồng ý nhận rất dễ
> khiến tài khoản bị Telegram giới hạn (`FLOOD_WAIT`) hoặc khoá. Giãn cách
> 3–10 giây là hợp lý.

---

## 7. Trò chuyện

| Việc | Cách làm |
|---|---|
| Trả lời một tin | Bấm đôi vào tin, hoặc bấm phải → *Trả lời* |
| Sửa tin của mình | Bấm phải → *Sửa* (rồi `Enter` để lưu, `Esc` để huỷ) |
| Xoá tin | Bấm phải → *Xoá*, chọn *chỉ ở máy tôi* hay *ở mọi người* |
| Chuyển tiếp | Bấm phải → *Chuyển tiếp*, chọn nơi nhận (kể cả tài khoản khác) |
| Chọn nhiều tin | Giữ `Ctrl` hoặc `Shift` rồi bấm; thanh công cụ hiện ra ở trên |
| Thả phản ứng | Bấm phải → *Phản ứng* → chọn emoji; chọn lại emoji đó để bỏ |
| Gửi nhãn dán | Bấm biểu tượng miếng dán cạnh mặt cười, chọn tab *Gần đây* hoặc *Yêu thích* |
| Gửi tệp | Bấm biểu tượng kẹp giấy, hoặc **kéo–thả** tệp vào ô soạn tin |
| Gửi ảnh trong clipboard | `Ctrl+V` ngay trong ô soạn tin |
| Tải ảnh / tệp về | Bấm vào ảnh hoặc thẻ tệp; bấm lại để mở |
| Mở thư mục chứa tệp | Bấm phải vào tin có tệp → *Mở thư mục chứa tệp* |
| Xem thông tin nhóm | Bấm biểu tượng ⓘ trên thanh tiêu đề (`Ctrl+I`) |
| Nhắn riêng một thành viên | Bảng thông tin → bấm đôi vào tên thành viên |
| Tìm trong cuộc trò chuyện | `Ctrl+Shift+F`, `Enter` để tìm, mũi tên để nhảy kết quả |

Nội dung đang soạn dở được **lưu thành nháp** khi bạn chuyển sang chat khác, và
đồng bộ với các thiết bị Telegram khác.

---

## 8. Cài đặt đáng chú ý

**Giao diện**: chủ đề tối / sáng / theo hệ thống, 7 màu nhấn, cỡ chữ 80–150%,
danh sách chat dạng gọn một dòng.

**Thông báo**: bật/tắt, hiện hay ẩn nội dung tin, biểu tượng khay hệ thống.
Bật *"Bấm X thì thu vào khay"* để đóng cửa sổ mà mọi tài khoản vẫn trực tuyến
và vẫn nhận thông báo.

**Tệp & bộ đệm**: tự tải ảnh, ngưỡng dung lượng tự tải, xem tổng dung lượng đã
dùng và **dọn bộ đệm** (không ảnh hưởng tin nhắn hay phiên đăng nhập).

**Proxy**: SOCKS5 / HTTP / MTProto, áp dụng cho **mọi tài khoản** cùng lúc —
hữu ích khi Telegram bị chặn.

**Nâng cao**: khoá API, đường dẫn TDLib, mức ghi log, mở lại toàn bộ kết nối.

---

## 9. Chuyển sang máy khác

Vì mọi thứ nằm trong thư mục `data` cạnh tệp chạy:

1. Thoát ứng dụng hoàn toàn (khay hệ thống → *Thoát*).
2. Copy **cả thư mục** ứng dụng sang máy mới / USB.
3. Chạy — mọi tài khoản vẫn đăng nhập, tin nhắn, cấu hình còn nguyên.

Muốn dùng thư mục dữ liệu khác:

```bash
./TuanMultiTeleClient --data-dir /duong/dan/khac
```

---

## 10. Gặp sự cố?

| Hiện tượng | Cách xử lý |
|---|---|
| "Chưa nạp TDLib" ở thanh dưới | Đặt `tdjson.dll` / `libtdjson.so` cạnh tệp chạy, hoặc trỏ đường dẫn ở *Cài đặt → Nâng cao*, rồi bấm **Nạp lại TDLib** |
| Không đăng nhập được, báo `FLOOD_WAIT` | Telegram tạm chặn vì thử quá nhiều lần. Đợi đúng số giây báo trong lỗi rồi thử lại |
| Kẹt ở "Đang kết nối…" | Mạng chặn Telegram → khai báo proxy ở *Cài đặt → Proxy* |
| Cửa sổ không mở trên Linux | Cài các gói `libxcb-*` ở mục 1, hoặc chạy `QT_QPA_PLATFORM=xcb ./chay-ung-dung.sh` |
| Dữ liệu không nằm cạnh tệp chạy | Thư mục hiện tại không cho ghi — chuyển ứng dụng sang Desktop hoặc USB |
| Muốn xem chuyện gì đang xảy ra | *Trợ giúp → Giới thiệu → Bản ghi hoạt động*, hoặc mở `data/logs/tuan-multitele.log` |

Khi báo lỗi, bấm **Chép thông tin chẩn đoán** trong hộp thoại *Giới thiệu* —
nó gom sẵn phiên bản, hệ điều hành, đường dẫn dữ liệu và trạng thái TDLib.

---

## 11. An toàn

- Thư mục `data/accounts` chứa **phiên đăng nhập Telegram**. Ai có thư mục đó
  là vào được tài khoản của bạn. Đừng để USB thất lạc, đừng copy lên máy lạ.
- Muốn "xoá dấu vết" trên một máy: thoát ứng dụng rồi xoá cả thư mục ứng dụng.
  Để cẩn thận hơn, **đăng xuất** trong ứng dụng trước khi xoá (huỷ phiên ở phía
  Telegram), hoặc vào *Telegram trên điện thoại → Cài đặt → Thiết bị* để chấm
  dứt phiên từ xa.
- `api_hash` và mật khẩu proxy nằm trong `data/config.ini` — chỉ được làm rối
  nhẹ, không phải mã hoá thật.
