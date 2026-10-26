# Minimum Spanning Tree with Union-Find

Chương trình C++ tìm cây khung nhỏ nhất bằng Kruskal và Union-Find.
Hỗ trợ chạy bằng console hoặc giao diện Windows.

## Biên dịch

Yêu cầu CMake 3.16 trở lên và trình biên dịch C++11. Giao diện dùng Win32,
chỉ được biên dịch trên Windows.

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Với Visual Studio, thêm `--config Debug` khi build và `-C Debug` khi chạy CTest.
Các file thực thi nằm trong `build/Debug/` thay vì `build/`.
Nếu dùng MinGW, cần có thư mục `bin` của MinGW trong PATH khi chạy.

## Console

```powershell
Get-Content examples/small_graph.txt | ./build/DSA.exe
```

Dữ liệu gồm `V E`, tiếp theo là đúng `E` bộ ba `from to weight`:

```text
3 3
0 1 2
1 2 3
0 2 6
```

- Đỉnh đánh số từ `0` đến `V-1`.
- Giới hạn: 1.000.000 đỉnh và 2.000.000 cạnh.
- Trọng số là số nguyên 32 bit; tổng trọng số dùng 64 bit.
- Chấp nhận trọng số âm, cạnh song song và cạnh tự khuyên.
- Đồ thị không liên thông trả về rừng khung nhỏ nhất và số thành phần liên thông.
- Khi nhập trực tiếp, kết thúc bằng Ctrl+Z rồi Enter trên Windows

## Giao diện

```powershell
./build/DSA_gui.exe
```

Nhập V, E và danh sách cạnh, mỗi cạnh một dòng. Nhấn **Run Kruskal** để xem
các cạnh được chọn, tổng trọng số và số thành phần liên thông.
**Load sample** nạp dữ liệu mẫu; **Clear** xóa dữ liệu và kết quả.

Giao diện nhận tối đa 16 Mi ký tự và hiển thị tối đa 10.000 cạnh kết quả.
Tổng trọng số và số thành phần vẫn được tính trên toàn bộ đồ thị.
Với dữ liệu lớn, nên dùng console; giao diện có thể tạm ngừng phản hồi khi tính toán.

Để chỉ biên dịch console và kiểm thử, cấu hình với `-DDSA_BUILD_GUI=OFF`.
