# Đánh giá docscan một cách định lượng

Tài liệu này mô tả cách đo chất lượng phát hiện tài liệu của docscan bằng **dữ liệu chuẩn công khai**,
**chỉ số đã được cộng đồng nghiên cứu dùng**, và **kiểm định thống kê**, để mọi kết luận
"phiên bản B tốt hơn A" đều do máy tính toán ra và người khác chạy lại được.

## 1. Nguyên tắc

1. **Dữ liệu chuẩn, có ground truth**: không đánh giá bằng vài ảnh tự chụp và nhìn bằng mắt.
2. **Chỉ số chuẩn**: dùng đúng giao thức của bộ dữ liệu, để so được với kết quả đã công bố.
3. **Tách dev / test**: chỉ tinh chỉnh tham số trên `dev`, chỉ báo cáo trên `test`. Tinh chỉnh trên chính
   tập dùng để chấm sẽ cho con số đẹp nhưng lạc quan giả.
4. **Khoảng tin cậy, không chỉ một con số**: một thay đổi chỉ được coi là cải thiện khi khoảng tin cậy
   95 % của *chênh lệch* nằm hẳn trên 0.
5. **Mỗi lần một thay đổi** (ablation): đo riêng tác dụng của từng ý tưởng.
6. **Ghi lại cấu hình**: mỗi lần chạy, `docscan-bench` ghi thêm `<results>.meta.json` (phiên bản, tham số).

## 2. Bộ dữ liệu

| Bộ dữ liệu | Nội dung | Dùng cho | Truy cập |
| --- | --- | --- | --- |
| **ICDAR 2015 SmartDoc, Challenge 1** | ~25 000 frame video Full HD; 6 loại tài liệu A4 × 5 nền (nền 5 khó nhất, nhiều vật thể); ground truth 4 góc | Phát hiện tài liệu giấy | Công khai, CC BY 4.0, ~1 GB: `npm run dataset:smartdoc15` |
| **MIDV-500 / MIDV-2019** | Video giấy tờ tùy thân (thẻ) giả lập, 50 loại | Phát hiện thẻ | Công khai (Smart Engines) |
| **MIDV-2020** | 1000 video, 2000 ảnh scan, 1000 ảnh chụp; 72 409 ảnh có chú thích, gồm biên giấy tờ | Phát hiện thẻ (bộ lớn nhất hiện nay) | Đăng ký qua form, ~124 GB |
| DocUNet benchmark, DIR300, UVDoc | Ảnh trang giấy cong/gấp và bản scan phẳng tương ứng | Nắn trang cong (chưa có trong docscan) | Công khai |
| DIBCO (2009–2019) | Ảnh văn bản và ảnh nhị phân chuẩn | Bộ lọc đen trắng `bw` | Công khai |

SmartDoc 2015 là điểm khởi đầu tốt nhất: nhỏ, giấy phép mở, có bảng kết quả của nhiều phương pháp.
Vì docscan nhắm tới cả **thẻ**, bước tiếp theo nên là MIDV-500 (hoặc MIDV-2020) với cùng giao thức đo.

## 3. Chỉ số (đã cài trong `apps/bench/metrics.cpp`, có unit test)

**Jaccard index theo giao thức SmartDoc** (chỉ số chính). Gọi *H* là homography đưa 4 góc ground truth về
hình chữ nhật *R* kích thước thật của tờ giấy. Chiếu tứ giác dự đoán *q* bằng *H* rồi tính

```
JI = area(H·q ∩ R) / area(H·q ∪ R)
```

Tính trên mặt phẳng tờ giấy nên kết quả không phụ thuộc kích thước hay phối cảnh của tài liệu trong ảnh.
Với dự đoán không hợp lệ (tự cắt, vượt đường chân trời) thì JI = 0. Frame không tìm thấy tài liệu được chấm
trên khung ảnh mà docscan trả về (toàn ảnh), giống việc một phương pháp bắt buộc phải trả kết quả cho mọi frame.

Các chỉ số phụ:

| Chỉ số | Ý nghĩa |
| --- | --- |
| Tỉ lệ frame có JI ≥ 0.90 / ≥ 0.95 | Tỉ lệ "cắt đúng"; trung bình JI có thể che giấu các lỗi nặng |
| Trung vị, phân vị 10 % của JI | Chất lượng ở phần "đuôi" (các frame khó) |
| Sai số góc (% đường chéo tờ giấy) | Dễ hiểu hơn JI: 1 % ≈ 3.6 mm trên A4 |
| AUROC của `confidence` | `confidence` có phân biệt được frame đúng (JI ≥ 0.95) với frame sai không; quan trọng cho UX (khi nào bắt người dùng chỉnh tay) |
| Thời gian p50 / p95 | Đo với `--threads 1`; chạy đa luồng làm số liệu thời gian sai lệch |

**Khoảng tin cậy**: bootstrap theo cụm (cluster bootstrap). Mỗi lần lấy mẫu lại là lấy lại *cả video*,
vì các frame trong một video gần như giống nhau và không phải mẫu độc lập. Coi 25 000 frame là độc lập
sẽ cho khoảng tin cậy hẹp giả. **So sánh hai phiên bản** được ghép cặp theo từng frame:
nếu khoảng tin cậy 95 % của ΔJI không chứa 0 thì chênh lệch có ý nghĩa thống kê.

## 4. Cách chạy

```bash
npm run dataset:smartdoc15                     # tải, kiểm checksum, giải nén, tạo manifest.csv
cmake --preset native-release && cmake --build --preset native-release

B=build/native-release/apps/bench/docscan-bench
$B data/smartdoc15-ch1/manifest.csv bench-results/baseline.csv            # toàn bộ
$B data/smartdoc15-ch1/manifest.csv bench-results/dev.csv --split dev --stride 5   # vòng lặp nhanh khi tinh chỉnh

npm run eval:report -- bench-results/baseline.csv --worst 20
npm run eval:report -- bench-results/new.csv --baseline bench-results/baseline.csv --by split
```

Quy trình cải thiện:

1. Chạy baseline, lưu `bench-results/baseline.csv`.
2. Xem bảng theo nền và danh sách `--worst`, mở các frame tệ nhất bằng `docscan-cli <ảnh> out.png --debug khung.jpg`, rồi phân loại nguyên nhân lỗi.
3. Sửa một nguyên nhân, đo trên `dev`.
4. Khi chốt, đo trên `test` với `--baseline` và chỉ giữ thay đổi nếu kết luận là *significant improvement*.

Để thêm bộ dữ liệu khác, viết một script trong `scripts/datasets/` xuất ra cùng định dạng manifest:
`image,group,category,split,ref_w,ref_h,tl_x,tl_y,tr_x,tr_y,br_x,br_y,bl_x,bl_y`.

## 5. Kết quả đã công bố trên SmartDoc 2015 Challenge 1 (để đối chiếu)

Mean Jaccard index theo nền, trích từ bảng so sánh trong bài LDRNet (Wu et al., 2022). Các dòng của cuộc thi
gốc lấy từ báo cáo ICDAR 2015 (Burie et al.).

| Phương pháp | Nền 1 | Nền 2 | Nền 3 | Nền 4 | Nền 5 | Tổng |
| --- | --- | --- | --- | --- | --- | --- |
| HU-PageScan (FCN, das Neves Jr. et al., 2020) | – | – | – | – | – | 0.9923 |
| LDRNet-1.4 (CNN nhẹ, thời gian thực) | 0.9877 | 0.9838 | 0.9862 | 0.9802 | 0.9858 | 0.9849 |
| SEECS-NUST-2 | 0.9832 | 0.9724 | 0.9830 | 0.9695 | 0.9478 | 0.9743 |
| LRDE | 0.9869 | 0.9775 | 0.9889 | 0.9837 | 0.8613 | 0.9716 |
| SmartEngines | 0.9885 | 0.9833 | 0.9897 | 0.9785 | 0.6884 | 0.9548 |
| NetEase | 0.9624 | 0.9552 | 0.9621 | 0.9511 | 0.2218 | 0.8820 |
| RPPDI-UPE | 0.8274 | 0.9104 | 0.9697 | 0.3649 | 0.2163 | 0.7408 |
| SEECS-NUST | 0.8875 | 0.8264 | 0.7832 | 0.7811 | 0.0113 | 0.7393 |

Lưu ý khi so sánh: các phương pháp trong cuộc thi chạy trên toàn bộ dữ liệu. Số trên tập `test` của
docscan chỉ là xấp xỉ đối chiếu, nhưng là con số trung thực vì không tinh chỉnh trên đó.

### Kết quả của docscan (2026-10-08, OpenCV 4.13, `docscan-bench` toàn bộ 24 889 frame)

| Phiên bản | Nền 1 | Nền 2 | Nền 3 | Nền 4 | Nền 5 | Tổng | JI ≥ 0.95 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| v1: baseline (`minAreaRatio` 0.1, cạnh trên ảnh xám) | 0.8995 | 0.2851 | 0.6810 | 0.1365 | 0.1107 | 0.4894 [0.431, 0.545] | 44.0 % |
| v2: `minAreaRatio` 0.05 + cạnh theo từng kênh màu | 0.9776 | 0.9649 | 0.9727 | 0.7519 | 0.3388 | 0.8694 [0.841, 0.896] | 76.7 % |

Trên riêng tập **test** (19 870 frame, không dùng để tinh chỉnh): v2 đạt **0.8707 [0.839, 0.898]**.
So ghép cặp với v1 được ΔJI = **+0.398 [0.343, 0.459]**, cải thiện có ý nghĩa; 9 866 frame tốt lên,
2 419 frame kém đi hơn 0.01. Thời gian detect p50 tăng từ 20 lên 30 ms (native, 16 luồng song song).

Vị trí so với bảng ở trên: nền 1–3 (0.96–0.98) đã ngang các phương pháp của cuộc thi. Nền 4 và 5 kéo
điểm tổng xuống (0.869), dưới SmartEngines/LRDE (0.95–0.97) và các mạng neural (≥ 0.98).

Nhật ký thí nghiệm (đều đo trên `dev`, 1/5 số frame):

1. **Giả thuyết**: nhiều frame bị báo "không tìm thấy" vì tờ giấy chỉ chiếm 10–16 % khung hình, dưới ngưỡng
   `minAreaRatio = 0.1`. **Kết quả**: 0.1 → 0.05: JI 0.555 → 0.739, Δ +0.184 [0.090, 0.293]. Ngưỡng 0.02 cho
   kết quả tương đương (0.739), nên chọn 0.05 vì ít nguy cơ bắt nhầm vật nhỏ hơn.
2. **Giả thuyết**: ở nền 4 (giấy trắng trên bàn gần trắng) viền giấy gần như không có tương phản độ sáng,
   chỉ khác nhau về sắc màu, nên thuật toán bắt nhầm bức ảnh *trong* trang. **Kết quả**: thêm Canny trên
   từng kênh B, G, R: JI 0.739 → 0.864, Δ +0.125 [0.056, 0.206]; nền 4 tăng 0.14 → 0.70.

Lỗi còn lại, theo phân tích các frame tệ nhất (`--worst`):

- **Nền 5** (bàn bừa bộn, dây cáp và bút đè lên trang): cạnh trang bị cắt đứt, tứ giác lớn nhất thường là vật khác.
  Hướng thử tiếp: tứ giác dựng từ đường Hough/LSD (chịu được cạnh bị che), và xếp hạng ứng viên theo độ tương
  phản màu trong/ngoài (Tropin et al. 2020).
- **Nền 4**: đã tìm thấy nhưng chỉ 34 % frame có JI ≥ 0.95, góc chưa chính xác. Hướng thử tiếp: tinh chỉnh
  góc ở độ phân giải gốc.

## 6. Còn CamScanner và các app thương mại?

CamScanner, Adobe Scan, Microsoft Lens **không công bố chỉ số đo được** trên bộ dữ liệu chuẩn nào.
Các bài "so sánh app" trên mạng chấm điểm kiểu 9/10, tức là cảm tính. Muốn so sánh khoa học với các app này
thì phải tự đo trên cùng dữ liệu:

- Các app không xuất toạ độ góc, nên không đo trực tiếp được JI. Cách khả thi là đo **theo tác vụ**:
  nạp cùng một bộ ảnh vào từng app, xuất kết quả, chạy OCR (ví dụ Tesseract) và tính
  **CER (character error rate)** so với văn bản gốc. SmartDoc 2015 Challenge 2 có ảnh chụp kèm văn bản chuẩn,
  rất hợp cho việc này.
- Vì phải thao tác tay trên app, nên chọn trước một mẫu ngẫu nhiên cố định (ví dụ 100 ảnh) và ghi lại phiên bản app.

## 7. Hướng cải thiện có căn cứ trong tài liệu

| Hướng | Căn cứ | Ghi chú |
| --- | --- | --- |
| Đề xuất cạnh bằng đường thẳng Hough / LSD, xếp hạng tứ giác theo đặc trưng cạnh + màu | Tropin et al., *Advanced Hough-based method for on-device document localization*, Computer Optics 2021: tốt nhất trên MIDV-500, nhì trên SmartDoc, chạy trên điện thoại | Giữ được góc bị che hoặc nằm ngoài khung |
| Xếp hạng ứng viên bằng độ tương phản màu trong/ngoài tứ giác (khoảng cách χ² histogram) | Tropin et al., *Approach for Document Detection by Contours and Contrasts*, ICPR 2020 | Thay đổi nhỏ trên pipeline cổ điển hiện tại |
| Phân đoạn trang bằng mạng tích chập rồi khớp tứ giác | HU-PageScan, IET Image Processing 2020: JI 0.9923 | Cần dữ liệu huấn luyện ngoài SmartDoc |
| Hồi quy trực tiếp góc + cạnh bằng CNN nhẹ | LDRNet, 2022: JI 0.9849, rất nhanh | Có thể chạy qua `cv::dnn` (ONNX) |
| Tinh chỉnh góc ở độ phân giải gốc | Hiện góc tìm trên ảnh 640px rồi phóng lên | Cải thiện sai số góc, ít tốn kém |

Mỗi hướng trên đều phải đi qua quy trình ở mục 4 trước khi được giữ lại.

## Tài liệu tham khảo

- Burie et al., *ICDAR2015 Competition on Smartphone Document Capture and OCR (SmartDoc)*, ICDAR 2015.
  Dữ liệu: https://github.com/jchazalon/smartdoc15-ch1-dataset · bộ chấm điểm gốc: https://github.com/jchazalon/smartdoc15-ch1-eval
- Wu et al., *LDRNet: Enabling Real-time Document Localization on Mobile Devices*, 2022. https://arxiv.org/abs/2206.02136
- das Neves Jr. et al., *HU-PageScan: a fully convolutional neural network for document page crop*, IET Image Processing 14(15), 2020.
- Tropin et al., *Approach for Document Detection by Contours and Contrasts*, ICPR 2020. https://arxiv.org/abs/2008.02615
- Tropin et al., *Advanced Hough-based method for on-device document localization*, Computer Optics, 2021. https://arxiv.org/abs/2106.09987
- Bulatov et al., *MIDV-2020: A Comprehensive Benchmark Dataset for Identity Document Analysis*, 2021. https://arxiv.org/abs/2107.00396
- Ma et al., *DocUNet: Document Image Unwarping via a Stacked U-Net*, CVPR 2018 (benchmark MS-SSIM, LD, CER).
