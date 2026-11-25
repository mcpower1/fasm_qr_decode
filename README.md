# fasm-qr-decode

QR Code Model 2 decoder in **32-bit x86 assembly (FASM)** for Windows.
No CRT, no external dependencies, no heap allocation – pure x86 bit-twiddling, GF(256) math, and `KERNEL32.DLL` in the CLI wrapper.

## Repository Contents

- **`qr32.inc` (Core decoder):** Takes an exact, unrotated `N×N` byte module matrix (`0` = light, nonzero = dark) and writes decoded binary payload bytes. Zero imports, zero system calls, zero heap allocation; uses static BSS scratch buffers (not reentrant / not thread-safe).
- **`qrdecode.asm` (Thin Win32 frontend):** Loads a file, parses `QRM1` or uncompressed `BI_RGB` BMPs, estimates polarity from corner pixels, locates an axis-aligned dark bounding box, samples candidate versions `1..40`, filters orientations via a 7×7 finder score (`>= 130/147`), and calls `qr_decode` on promising candidates. Imports only `KERNEL32.DLL` via `win32a.inc`.
- **`build_qr.bat`:** Build script for Windows / FASM.
- **`hello.bmp` & `hello.qrm`:** Sample inputs (`HELLO WORLD`) for immediate sanity-checking without building a test matrix first.

## Build & Run

```bat
build_qr.bat
```

Or assemble directly:

```bat
fasm.exe qrdecode.asm qrdecode.exe
```

The resulting 32-bit console executable runs natively on 32-bit Windows and via WoW64 on Windows x64. Neither FASM nor third-party DLLs are required at runtime.

### CLI Usage

```bat
qrdecode.exe hello.bmp
qrdecode.exe hello.qrm
qrdecode.exe hello.bmp --raw
```

- **Default:** Writes payload bytes as **uppercase HEX** to `stdout` with trailing `CRLF` (binary-safe, including `0x00` bytes).
- **`--raw`:** Writes raw payload bytes directly to `stdout` without HEX encoding.
- **Errors:** Messages go to `stderr`, exit code `1`.

Expected output on the bundled test files:
- `qrdecode.exe hello.bmp` -> `48454C4C4F20574F524C44`
- `qrdecode.exe hello.bmp --raw` -> `HELLO WORLD`
- `qrdecode.exe hello.qrm` -> `48454C4C4F20574F524C44`

### Supported Inputs

- **Uncompressed Windows BMP (`BI_RGB`):** 1, 8, 24, or 32 bpp, bottom-up and top-down (grayscale threshold `< 128` on RGB average; auto-inverts if at least 3 of 4 corners are dark).
- **Axis-aligned bounding-box sampler:** Expects an isolated, roughly square (within 7% aspect tolerance), axis-aligned QR region.
- **Raw matrix (`.qrm`):** 4-byte ASCII `QRM1` + `uint32` little-endian side length `N` (`21, 25, 29, ..., 177`), followed by `N×N` bytes (`0` = light, nonzero = dark, row-major, no padding).

PNG, JPEG, TIFF, PDF objects, perspective distortion, or cluttered scenes are not handled by the bundled frontend. Bring your own rasterizer/sampler to feed a module matrix into `qr32.inc`.

## Core Internals (`qr32.inc`)

- **Versions & ECC:** QR Code Model 2, Versions 1–40 (`21×21` to `177×177`), ECC levels `L / M / Q / H`, all 8 mask patterns.
- **Format & layout:** Both 15-bit format copies checked against all 32 `BCH(15,5)` codewords (`0x537` poly, `0x5412` mask, up to 3 bit errors); function module map (`qr_function`) for timing, finders, alignment (including version 32 step `26` and finder corner skips), format, and version fields; zigzag codeword extraction and RS block deinterleaving.
- **Reed–Solomon over GF(256) (`0x11D`):** Lazy `qr_gf_exp` / `qr_gf_log` tables, syndromes, Berlekamp–Massey, Chien search, Gaussian elimination on the Vandermonde system, error application, and post-correction syndrome verification.
- **Segments & metadata:** Numeric, Alphanumeric (with FNC1 `%` -> `0x1D` GS / `%%` -> `%` handling), Byte, Kanji (Shift-JIS byte pairs), Hanzi (subset 1, GB2312 byte pairs), ECI (`qr_eci`, default `3`), FNC1 (1st/2nd), and Structured Append (`qr_sa_index`, `qr_sa_total`, `qr_sa_parity`).

**Scope & limitations:** No charset/Unicode transcoding; no multi-symbol Structured Append reassembly; Model 2 only (no Micro QR or rMQR).

## qr32.inc Interface (qr_decode)**

`stdcall`-style convention (`ret 16`), preserving `EBX`, `ESI`, `EDI`, `EBP`:

```asm
; Row-major module matrix (0 = light, !=0 = dark)
; N = 21 + 4*(Version-1), valid range: 21..177
push 4096                  ; output buffer capacity in bytes
push qr_payload_buffer     ; pointer to binary output buffer
push [qr_side]             ; matrix side length N
push [qr_matrix_ptr]       ; pointer to N*N bytes
call qr_decode
; EAX >= 0: decoded payload byte count
; EAX <  0: negative error code
```

| Code | Meaning |
|---|---|
| `-1` | Invalid arguments |
| `-2` | Unreadable format info |
| `-3` | Uncorrectable Reed–Solomon errors |
| `-4` | Invalid or unsupported data segments |
| `-5` | Output buffer too small |
| `-6` | Invalid function or codeword layout |

## Origin of ISO Tables

Error correction block tables and baseline rules were cross-checked against Project Nayuki's open-source **QR Code generator** (MIT License):
https://github.com/nayuki/QR-Code-generator

The FASM decoder itself is implemented as standalone assembly code.
