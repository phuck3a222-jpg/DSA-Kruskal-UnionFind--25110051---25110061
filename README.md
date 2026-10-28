# Minimum Spanning Tree with Union-Find

## Yêu cầu

- Windows.
- CLion và trình biên dịch C++11 (MinGW hoặc Visual Studio).

## Chạy bằng CLion

1. Mở CLion, chọn **Open**.
2. Chọn thư mục dự án có chứa `CMakeLists.txt`.
3. Chờ CLion tải xong cấu hình CMake.
4. Ở danh sách cấu hình chạy, chọn **DSA_gui**.
5. Nhấn nút **Run** hoặc **Shift + F10** để mở giao diện.

Muốn chạy chương trình console, chọn **DSA** thay cho **DSA_gui**.

## Chạy bằng PowerShell

Mở PowerShell tại thư mục dự án. Cần có CMake và trình biên dịch trong PATH.

```powershell
cmake -S . -B build
cmake --build build --target DSA_gui --config Release
```

Nếu dùng MinGW, chạy:

```powershell
.\build\DSA_gui.exe
```

Nếu dùng Visual Studio, chạy:

```powershell
.\build\Release\DSA_gui.exe
```
