#!/usr/bin/env python3
"""Tự tăng số phiên bản trong tệp VERSION.txt.

Dùng ở hai nơi:

  * git hook pre-commit  — tăng PATCH khi có thay đổi trong src/, resources/,
    CMakeLists.txt hoặc cmake/ (xem scripts/hooks/pre-commit);
  * GitHub Actions       — tăng PATCH cho mỗi lần đẩy mã lên nhánh.

Cách dùng:

    python3 scripts/bump_version.py --part patch
    python3 scripts/bump_version.py --auto --diff-base HEAD~1
    python3 scripts/bump_version.py --print

Tệp VERSION.txt chứa đúng một dòng dạng MAJOR.MINOR.PATCH.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
VERSION_FILE = REPO_ROOT / "VERSION.txt"

# Chỉ những thay đổi ở các đường dẫn này mới coi là "cập nhật mã nguồn".
WATCHED_PREFIXES = (
    "src/",
    "resources/",
    "cmake/",
    "packaging/",
    "CMakeLists.txt",
)

VERSION_PATTERN = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")


def read_version() -> tuple[int, int, int]:
    if not VERSION_FILE.exists():
        return (0, 1, 0)
    raw = VERSION_FILE.read_text(encoding="utf-8").strip()
    match = VERSION_PATTERN.match(raw)
    if not match:
        raise SystemExit(f"VERSION.txt không hợp lệ: {raw!r} (cần MAJOR.MINOR.PATCH)")
    return tuple(int(part) for part in match.groups())  # type: ignore[return-value]


def write_version(version: tuple[int, int, int]) -> str:
    text = ".".join(str(part) for part in version)
    VERSION_FILE.write_text(text + "\n", encoding="utf-8")
    return text


def bump(version: tuple[int, int, int], part: str) -> tuple[int, int, int]:
    major, minor, patch = version
    if part == "major":
        return (major + 1, 0, 0)
    if part == "minor":
        return (major, minor + 1, 0)
    return (major, minor, patch + 1)


ZERO_SHA = "0" * 40


def run_git(args: list[str]) -> str | None:
    """Chạy git; trả về None nếu lệnh lỗi (khác với chạy được mà không có kết quả)."""
    result = subprocess.run(
        ["git", *args],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    return result.stdout if result.returncode == 0 else None


def changed_paths(staged: bool, diff_base: str | None) -> list[str] | None:
    """Danh sách tệp đã thay đổi.

    Trả về None khi không xác định được (nhánh mới nên không có mốc so sánh,
    lịch sử bị cắt ngắn…). Người gọi coi None là "cứ tăng phiên bản cho chắc",
    vì một lần đẩy mã không phân tích được thì phần lớn là có sửa mã.
    """
    if staged:
        output = run_git(["diff", "--cached", "--name-only"])
        return _split(output) if output is not None else None

    candidates: list[list[str]] = []
    if diff_base and diff_base.strip() and diff_base.strip(ZERO_SHA[0]) != "":
        candidates.append(["diff", "--name-only", f"{diff_base}..HEAD"])
    candidates.append(["diff", "--name-only", "HEAD~1..HEAD"])
    # Commit đầu tiên không có HEAD~1 — liệt kê thẳng nội dung commit.
    candidates.append(["show", "--name-only", "--pretty=format:", "HEAD"])

    for args in candidates:
        output = run_git(args)
        if output is None:
            continue
        paths = _split(output)
        if paths:
            return paths
    return None


def _split(output: str) -> list[str]:
    return [line.strip() for line in output.splitlines() if line.strip()]


def source_touched(paths: list[str] | None) -> bool:
    if paths is None:
        return True   # không rõ thì cứ tăng
    return any(path.startswith(WATCHED_PREFIXES) for path in paths)


def main() -> int:
    parser = argparse.ArgumentParser(description="Tăng phiên bản ứng dụng.")
    parser.add_argument(
        "--part",
        choices=["major", "minor", "patch"],
        default="patch",
        help="Thành phần cần tăng (mặc định: patch).",
    )
    parser.add_argument(
        "--auto",
        action="store_true",
        help="Chỉ tăng nếu có thay đổi trong mã nguồn.",
    )
    parser.add_argument(
        "--staged",
        action="store_true",
        help="Với --auto: xét các tệp đang staged (dùng cho git hook).",
    )
    parser.add_argument(
        "--diff-base",
        default=None,
        help="Với --auto: so sánh từ mốc này tới HEAD (mặc định HEAD~1).",
    )
    parser.add_argument(
        "--print",
        dest="print_only",
        action="store_true",
        help="Chỉ in phiên bản hiện tại rồi thoát.",
    )
    args = parser.parse_args()

    current = read_version()

    if args.print_only:
        print(".".join(str(part) for part in current))
        return 0

    if args.auto:
        paths = changed_paths(args.staged, args.diff_base)
        if paths is None and not args.staged:
            print("bump_version: không xác định được thay đổi, tăng phiên bản cho chắc")

        # Nếu chính tệp phiên bản đã đổi trong khoảng so sánh thì git hook cục bộ
        # (hoặc người dùng) đã tăng rồi — CI tăng thêm lần nữa sẽ nhảy số vô ích.
        if paths is not None and not args.staged and VERSION_FILE.name in paths:
            print(
                "bump_version: phiên bản đã được tăng trong lần đẩy này, giữ nguyên "
                + ".".join(str(part) for part in current)
            )
            return 0

        if not source_touched(paths):
            print(
                "bump_version: không có thay đổi trong mã nguồn, giữ nguyên "
                + ".".join(str(part) for part in current)
            )
            return 0

    new_version = write_version(bump(current, args.part))
    print(f"bump_version: {'.'.join(str(p) for p in current)} -> {new_version}")

    # Với git hook, đưa luôn VERSION vào commit đang tạo.
    if args.staged:
        subprocess.run(["git", "add", "VERSION.txt"], cwd=REPO_ROOT, check=False)

    return 0


if __name__ == "__main__":
    sys.exit(main())
