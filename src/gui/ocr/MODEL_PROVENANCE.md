# Built-in OCR model provenance

The runtime embeds two ONNX models converted from immutable Paddle inference
snapshots. Conversion is intentionally outside the application and RPM builds.

## Source snapshots

| Role | Repository revision | Paddle graph SHA-256 | Paddle parameters SHA-256 | Metadata SHA-256 |
| --- | --- | --- | --- | --- |
| Detection | `PaddlePaddle/PP-OCRv5_mobile_det@0d63e78e2b680928f6b1747d76a08db6e645efb7` | `05feef1acb00aa4cd7362b15f7f501fc4f99d7b1fa73c1c871e0c7b1504b0f5c` | `afa1820cb16c1fd0dad589d0f8b389139061c1ef6d68019685fd07be997dda5b` | `98069072e1b6b37d727fd9d9f11725faa46d6ea0de012f2ed26caea011c37699` |
| Recognition | `PaddlePaddle/eslav_PP-OCRv5_mobile_rec@7553801264d3379d8d2e854971989e5e26c22e03` | `3fb6e2e658f5139ff16e35260de8f0577f106a9505c902e1dfc1f4f1d03cc9cb` | `f11057b05d8517868bca505271278973d706600d9dcc184cbcf5c4512091c32b` | `025039bac23eb4a308efcefa4d58eab3af440767815c6ba6938468bf6353ee5a` |

Both model cards declare Apache-2.0. The retained `inference.yml` files are
build provenance and are not loaded at runtime.

## Conversion

Environment: CPython 3.12.12, PaddlePaddle 3.2.2, Paddle2ONNX 2.1.0,
ONNX 1.17.0, ONNX Runtime 1.27.0, NumPy 2.2.6.

Each snapshot was converted with:

```text
paddle2onnx --model_dir <snapshot> \
  --model_filename inference.json \
  --params_filename inference.pdiparams \
  --save_file inference.onnx \
  --opset_version 17 \
  --enable_auto_update_opset True \
  --enable_onnx_checker True \
  --optimize_tool None
```

Final artifacts:

| Artifact | Size | SHA-256 |
| --- | ---: | --- |
| `det/inference.onnx` | 4,766,440 | `c8d9b07063420ce5365c74e42532de48238feeeedcdb7a330b195708bc38a93f` |
| `rec/inference.onnx` | 7,882,715 | `a966b18ae06292f6df1114183fa69db2fb31ab62d930d73b44b8d1bef0ae77ff` |
| `dictionary.bin` | 1,663 | `f155bbac2ce943f7138f0dcfd1a0cc0821c743579e9014da627c219bbe689b14` |

`dictionary.bin` is the recognizer metadata's ordered character list encoded
as NUL-separated UTF-8 entries. Runtime decoding prepends CTC blank and appends
the configured ASCII space exactly as the pinned PaddleOCR processor does.

## Verification

The Paddle and ONNX paths retain dynamic input shapes. Identical tensors pass
at `rtol=1e-4, atol=1e-4`; threshold maps, detector boxes, CTC IDs, and decoded
fixture text match exactly. The static production pipeline reproduces the
English, Russian, and mixed fixtures. Known Latin/Cyrillic homoglyph errors in
mixed text are model output and are not rewritten heuristically.

Processing behavior was adapted from PaddleOCR commit
`2661c7c0ef5c613e8f93c6e93b2e052399f0f854` under Apache-2.0. Polygon expansion
uses Clipper 6.4.2 under BSL-1.0.
