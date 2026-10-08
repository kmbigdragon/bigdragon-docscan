# CI/CD và quy trình phát hành

```
 push / pull request ──► CI (ci.yml) ─────────────┐
                                                  ├─► build.yml (dùng chung)
 git tag vX.Y.Z ───────► Release (release.yml) ───┘     ├─ native: C++ core + CLI + bench, GoogleTest (Linux)
        │                                               └─ wasm:   OpenCV-wasm (cache) → docscan.wasm → TypeScript
        │                                                          → npm test → npm pack → smoke test → artifact
        └─ kiểm tra tag == package.json ──► publish: GitHub Release + docscan-X.Y.Z.tgz + .sha256
```

| Workflow | Khi nào chạy | Kết quả |
| --- | --- | --- |
| `ci.yml` | Push lên `main`, mọi pull request, chạy tay | Đỏ/xanh; tarball của lần chạy nằm trong mục *Artifacts* |
| `release.yml` | Push tag `v*.*.*` | GitHub Release có tarball cài được |
| `benchmark.yml` | Chạy tay (Actions → Benchmark → Run workflow) | Báo cáo chất lượng SmartDoc trong *summary* của lần chạy |

Release và CI dùng chung `build.yml`, nên bản phát hành được build đúng như các lần CI đã qua.

## Trước mỗi commit: husky + commitlint

`npm install` tự bật các git hook trong `.husky/` (qua script `prepare`).

| Hook | Kiểm tra |
| --- | --- |
| `pre-commit` → `scripts/hooks/pre-commit.mjs` | Chỉ kiểm tra những gì liên quan tới file đang stage: cú pháp JSON/JS; `tsc --noEmit` khi đổi `js/`; build C++ tăng dần + `ctest` khi đổi C++/CMake (cần đã chạy `cmake --preset native` một lần); test JS (build lại wasm nếu đổi `bindings/`); `actionlint` nếu đã cài |
| `commit-msg` → commitlint | Message theo [Conventional Commits](https://www.conventionalcommits.org): `<type>(<scope>): <subject>` |

Các type hợp lệ: `feat`, `fix`, `perf`, `refactor`, `docs`, `test`, `build`, `ci`, `chore`, `style`, `revert`.
Ví dụ: `feat(detect): hough-based candidates`, `fix(wasm): keep RGBA layout`, `docs: release steps`.
Subject viết thường ở đầu và không có dấu chấm cuối. Thay đổi phá vỡ API thì thêm `!`: `feat(api)!: rename scan()`.

`.npmrc` đặt message của `npm version` thành `chore(release): X.Y.Z` để qua được commitlint.
Trên CI, mọi commit của pull request được commitlint kiểm tra lại, vì hook ở máy có thể bị bỏ qua bằng
`git commit --no-verify`.

## Phát hành một phiên bản

Điều kiện: CI trên `main` đang xanh, working tree sạch.

Trước khi tag, viết phần giới thiệu bản phát hành vào `docs/releases/vX.Y.Z.md` (xem `v0.1.0.md`) rồi commit.
Workflow sẽ dùng file đó làm nội dung trang GitHub Release, sau đó nối thêm danh sách thay đổi do GitHub tự sinh.

```bash
npm version 0.2.0          # sửa package.json + package-lock.json, tạo commit "chore(release): 0.2.0" và tag v0.2.0
git push origin main --follow-tags
gh run watch               # (tuỳ chọn) theo dõi workflow Release
```

- Có thể dùng `npm version patch|minor|major` thay cho số cụ thể.
- Bản thử nghiệm: `npm version 0.3.0-rc.1` sinh tag `v0.3.0-rc.1`, được đánh dấu *pre-release* trên GitHub.
- Phiên bản chỉ khai báo ở `package.json`; CMake đọc từ đó, nên `scanner.version` / `docscan::version()` luôn khớp.

Workflow Release lần lượt:

1. Từ chối nếu tag khác `v` + version trong `package.json`.
2. Build và test toàn bộ.
3. Đóng gói, rồi cài tarball vào một dự án trắng và gọi thử API (smoke test).
4. Tạo GitHub Release gồm `docscan-X.Y.Z.tgz`, `docscan-X.Y.Z.tgz.sha256` và ghi chú cài đặt.

Hai lớp bảo vệ để không phát hành nhầm:

- `prepack` (`scripts/release/check-dist.mjs`) từ chối `npm pack` khi chưa build `dist/`, hoặc khi `dist/` được build cho một version khác.
- `scripts/release/check-tag.mjs` từ chối tag không khớp version.

### Kiểm tra trên máy trước khi tag (khuyến nghị)

```bash
npm run build && npm test
npm pack                                   # → docscan-X.Y.Z.tgz
npm run release:smoke -- docscan-X.Y.Z.tgz # cài vào dự án trắng và gọi thử
```

### Rút lại một bản phát hành lỗi

```bash
gh release delete v0.2.0 --cleanup-tag --yes
```

**Không dùng lại số version đã phát hành**: dự án khác có thể đã ghi checksum của tarball cũ trong lock file.
Hãy sửa lỗi rồi phát hành `0.2.1`.

## Cài đặt từ tarball

```bash
# trực tiếp từ GitHub Release (repo public, không cần đăng nhập)
npm install https://github.com/kmbigdragon/bigdragon-docscan/releases/download/v0.2.0/docscan-0.2.0.tgz

# hoặc tải về, kiểm tra checksum rồi cài file cục bộ
gh release download v0.2.0 --repo kmbigdragon/bigdragon-docscan
sha256sum -c docscan-0.2.0.tgz.sha256
npm install ./docscan-0.2.0.tgz
```

Trong `package.json` của dự án dùng thư viện, dependency sẽ có dạng
`"docscan": "https://github.com/kmbigdragon/bigdragon-docscan/releases/download/v0.2.0/docscan-0.2.0.tgz"`.
Để nâng cấp, đổi URL sang tag mới. `package-lock.json` ghi lại integrity hash của tarball, nên các lần cài sau
được xác minh tự động.

## Bảo trì pipeline

- **Phiên bản Emscripten** ghim tại `EMSDK_VERSION` trong `.github/workflows/build.yml` (hiện `6.0.10`).
  Giữ trùng với bản trên máy dev; đổi giá trị này sẽ làm mất cache và build lại OpenCV-wasm.
- **Phiên bản OpenCV cho wasm** ghim trong `scripts/build-opencv-wasm.mjs` (hiện `4.13.0`). Cache OpenCV-wasm
  có khoá theo hash của file này, nên mọi thay đổi cờ build sẽ tự build lại.
- **Job native** dùng OpenCV của Ubuntu (`libopencv-dev`, bản 4.6) để kiểm tra mã C++ biên dịch và chạy đúng
  trên phiên bản cũ hơn. Gói phát hành không phụ thuộc vào bản này: tarball chỉ chứa wasm build với OpenCV ghim ở trên.
- **Cache**: lần chạy đầu mất thêm vài phút để build OpenCV-wasm, các lần sau dùng lại cache.
- **Benchmark trên CI** chạy bằng OpenCV của runner (4.6), nên số liệu có thể lệch nhẹ so với số đo trên máy
  (`docs/EVALUATION.md`). Chỉ so sánh các số đo cùng môi trường với nhau.

## Việc nên làm thêm

- Thêm file `LICENSE`: `package.json` khai báo MIT nhưng repo chưa có văn bản giấy phép (cần tên chủ bản quyền).
- Nếu sau này muốn `npm install docscan` từ npm registry: đổi tên gói (tên `docscan` có thể đã bị dùng,
  nên đặt tên dạng `@ten-ban/docscan`), thêm secret `NPM_TOKEN`, và thêm bước `npm publish --provenance` vào `release.yml`.
