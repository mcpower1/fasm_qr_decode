; qrdecode.asm -- Windows x86 QR Code decoder without third-party DLLs
; Build: fasm qrdecode.asm qrdecode.exe
; Usage: qrdecode.exe "qr.bmp" [--raw]
;        qrdecode.exe "qr.qrm" [--raw]   ; QRM1 + u32 LE size + N*N bits
; By default stdout contains uppercase HEX + CRLF, --raw writes binary bytes.
; BMP formats: uncompressed 1/8/24/32bpp, axis-aligned, single QR image.
; The independent QR core (qr32.inc) supports standard Model 2 versions 1..40.
format PE console 4.0
entry qd_start
include 'win32a.inc'

QD_MAXFILE = 33554432
QD_MAXDIM = 8192

section '.data' data readable writeable
qd_usage db 'Usage: qrdecode.exe "qr.bmp|qr.qrm" [--raw]',13,10,0
qd_errfile db 'Error: file could not be loaded.',13,10,0
qd_errformat db 'Error: expected QRM1 or uncompressed Windows BMP (1/8/24/32 bpp).',13,10,0
qd_errdecode db 'Error: no decodable axis-aligned QR Code Model 2 found.',13,10,0
qd_errmemory db 'Error: allocation failed.',13,10,0
qd_hexchars db '0123456789ABCDEF'
qd_filename rb 1024
qd_option rb 64
qd_argmax dd 0
qd_hfile dd -1
qd_stdout dd 0
qd_stderr dd 0
qd_filesize dd 0
qd_fileptr dd 0
qd_nread dd 0
qd_nwritten dd 0
qd_raw_output dd 0
qd_source dd 0
qd_side dd 0
qd_rotation dd 0
qd_output_len dd 0
qd_bmp_w dd 0
qd_bmp_h dd 0
qd_bmp_topdown dd 0
qd_bmp_bpp dd 0
qd_bmp_stride dd 0
qd_bmp_pixels dd 0
qd_bmp_palette dd 0
qd_bmp_palette_len dd 0
qd_bmp_threshold dd 128
qd_bmp_invert dd 0
qd_minx dd 0
qd_maxx dd 0
qd_miny dd 0
qd_maxy dd 0
qd_bbox_w dd 0
qd_bbox_h dd 0
qd_scanx dd 0
qd_scany dd 0
qd_tx dd 0
qd_ty dd 0
qd_imgx dd 0
qd_imgy dd 0
qd_finderid dd 0
qd_finderx dd 0
qd_findery dd 0
qd_fx dd 0
qd_fy dd 0
qd_score dd 0
qd_candidate_score dd 0
qd_version dd 0
qd_transform_x dd 0
qd_transform_y dd 0
qd_src_x dd 0
qd_src_y dd 0
qd_matrix rb 31329
qd_rotated rb 31329
qd_payload rb 8192
qd_hexout rb 16386

section '.code' code readable executable
qd_start:
 cld
 invoke GetStdHandle,-11
 mov [qd_stdout],eax
 invoke GetStdHandle,-12
 mov [qd_stderr],eax
 invoke GetCommandLineA
 mov esi,eax
 ; Skip executable filename, allowing quotes around it.
 mov [qd_argmax],63
 mov edi,qd_option
 call qd_next_arg
 mov [qd_argmax],1023
 mov edi,qd_filename
 call qd_next_arg
 jc .usage
 mov [qd_argmax],63
 mov edi,qd_option
 call qd_next_arg
 jc .options_done
 cmp byte [qd_option],'-'
 jne .options_done
 cmp byte [qd_option+1],'-'
 jne .options_done
.options_done:
 ; Only --raw changes the output format.
 cmp byte [qd_option],'-'
 jne .open
 cmp byte [qd_option+1],'-'
 jne .open
 cmp byte [qd_option+2],'r'
 jne .open
 cmp byte [qd_option+3],'a'
 jne .open
 cmp byte [qd_option+4],'w'
 jne .open
 cmp byte [qd_option+5],0
 jne .open
 mov [qd_raw_output],1
.open:
 invoke CreateFileA,qd_filename,80000000h,1,0,3,80h,0
 cmp eax,-1
 je .file_error
 mov [qd_hfile],eax
 invoke GetFileSize,eax,0
 cmp eax,-1
 je .file_error
 cmp eax,8
 jb .file_error
 cmp eax,QD_MAXFILE
 ja .file_error
 mov [qd_filesize],eax
 invoke GlobalAlloc,40h,eax
 test eax,eax
 jz .mem_error
 mov [qd_fileptr],eax
 invoke ReadFile,[qd_hfile],eax,[qd_filesize],qd_nread,0
 test eax,eax
 jz .file_error
 mov eax,[qd_nread]
 cmp eax,[qd_filesize]
 jne .file_error
 invoke CloseHandle,[qd_hfile]
 mov [qd_hfile],-1
 mov esi,[qd_fileptr]
 cmp byte [esi],'Q'
 jne .bmp
 cmp byte [esi+1],'R'
 jne .bmp
 cmp byte [esi+2],'M'
 jne .bmp
 cmp byte [esi+3],'1'
 jne .bmp
 mov eax,[esi+4]
 mov [qd_side],eax
 cmp eax,21
 jb .bad_format
 cmp eax,177
 ja .bad_format
 mov ecx,eax
 sub ecx,17
 test ecx,3
 jnz .bad_format
 imul eax,eax
 add eax,8
 cmp eax,[qd_filesize]
 ja .bad_format
 mov eax,[qd_fileptr]
 add eax,8
 mov [qd_source],eax
 call qd_try_orientations
 test eax,eax
 jns .success
 jmp .no_qr
.bmp:
 call qd_bmp_init
 jc .bad_format
 call qd_find_bbox
 jc .no_qr
 mov [qd_version],1
.try_versions:
 cmp [qd_version],40
 ja .no_qr
 mov eax,[qd_version]
 shl eax,2
 add eax,17
 mov [qd_side],eax
 cmp eax,[qd_bbox_w]
 ja .next_version
 cmp eax,[qd_bbox_h]
 ja .next_version
 call qd_sample_bbox
 call qd_try_orientations
 test eax,eax
 jns .success
.next_version:
 inc [qd_version]
 jmp .try_versions
.success:
 mov [qd_output_len],eax
 cmp [qd_raw_output],0
 jne .write_raw
 mov esi,qd_payload
 mov edi,qd_hexout
 mov ecx,[qd_output_len]
.hex_loop:
 test ecx,ecx
 jz .hex_done
 movzx eax,byte [esi]
 mov edx,eax
 shr eax,4
 mov al,[qd_hexchars+eax]
 mov [edi],al
 and edx,15
 mov dl,[qd_hexchars+edx]
 mov [edi+1],dl
 add edi,2
 inc esi
 dec ecx
 jmp .hex_loop
.hex_done:
 mov word [edi],0A0Dh
 add edi,2
 sub edi,qd_hexout
 invoke WriteFile,[qd_stdout],qd_hexout,edi,qd_nwritten,0
 jmp .exit_ok
.write_raw:
 invoke WriteFile,[qd_stdout],qd_payload,[qd_output_len],qd_nwritten,0
.exit_ok:
 invoke ExitProcess,0
.usage:
 mov edx,qd_usage
 jmp .print_error
.file_error:
 mov edx,qd_errfile
 jmp .print_error
.mem_error:
 mov edx,qd_errmemory
 jmp .print_error
.bad_format:
 mov edx,qd_errformat
 jmp .print_error
.no_qr:
 mov edx,qd_errdecode
.print_error:
 call qd_write_error
 invoke ExitProcess,1

; Extract one space- or quote-delimited argument from GetCommandLineA.
; ESI = cursor, EDI = 1024/64 byte destination; CF set if absent.
qd_next_arg:
.skip:
 cmp byte [esi],' '
 je .white
 cmp byte [esi],9
 jne .begin
.white:
 inc esi
 jmp .skip
.begin:
 cmp byte [esi],0
 je .none
 mov bl,0
 cmp byte [esi],34
 jne .copy
 mov bl,1
 inc esi
.copy:
 xor ecx,ecx
.loop:
 mov al,[esi]
 test al,al
 jz .finish
 cmp bl,0
 jne .quoted
 cmp al,' '
 je .finish
 cmp al,9
 je .finish
 jmp .put
.quoted:
 cmp al,34
 je .closequote
.put:
 cmp ecx,[qd_argmax]
 jae .skip_excess
 mov [edi+ecx],al
 inc ecx
.skip_excess:
 inc esi
 jmp .loop
.closequote:
 inc esi
.finish:
 mov byte [edi+ecx],0
 clc
 ret
.none:
 mov byte [edi],0
 stc
 ret

qd_write_error:
 push edx
 mov esi,edx
 xor ecx,ecx
.len:
 cmp byte [esi+ecx],0
 je .write
 inc ecx
 jmp .len
.write:
 invoke WriteFile,[qd_stderr],edx,ecx,qd_nwritten,0
 pop edx
 ret

; Validate BMP payload before touching pixels. Only BI_RGB images.
qd_bmp_init:
 mov esi,[qd_fileptr]
 cmp word [esi],4D42h
 jne .fail
 cmp [qd_filesize],54
 jb .fail
 mov eax,[esi+14]
 cmp eax,40
 jb .fail
 cmp eax,124
 ja .fail
 mov edx,eax
 add edx,14
 cmp edx,[qd_filesize]
 ja .fail
 mov eax,[esi+18]
 cmp eax,1
 jl .fail
 cmp eax,QD_MAXDIM
 ja .fail
 mov [qd_bmp_w],eax
 mov eax,[esi+22]
 test eax,eax
 jz .fail
 mov [qd_bmp_topdown],0
 jns .height
 neg eax
 mov [qd_bmp_topdown],1
.height:
 cmp eax,QD_MAXDIM
 ja .fail
 mov [qd_bmp_h],eax
 cmp word [esi+26],1
 jne .fail
 cmp dword [esi+30],0
 jne .fail
 movzx eax,word [esi+28]
 cmp eax,1
 je .bpp_ok
 cmp eax,8
 je .bpp_ok
 cmp eax,24
 je .bpp_ok
 cmp eax,32
 jne .fail
.bpp_ok:
 mov [qd_bmp_bpp],eax
 mov ebx,[qd_bmp_w]
 imul eax,ebx
 add eax,31
 shr eax,5
 shl eax,2
 mov [qd_bmp_stride],eax
 imul eax,[qd_bmp_h]
 mov ecx,[esi+10]
 cmp ecx,54
 jb .fail
 mov edx,ecx
 add edx,eax
 jc .fail
 cmp edx,[qd_filesize]
 ja .fail
 add ecx,esi
 mov [qd_bmp_pixels],ecx
 mov eax,[esi+14]
 lea eax,[esi+eax+14]
 mov [qd_bmp_palette],eax
 cmp ecx,eax
 jb .fail
 mov ebx,ecx
 sub ebx,eax
 shr ebx,2
 mov [qd_bmp_palette_len],ebx
 clc
 ret
.fail:
 stc
 ret

; Coordinates (ECX=x, EDX=y) -> EAX=1 dark / 0 light.
; Returns dark according to threshold 128. Clip is done by callers.
qd_getpixel:
 push ebx
 push ecx
 push edx
 push esi
 push edi
 mov eax,[qd_bmp_h]
 cmp [qd_bmp_topdown],0
 jne .topdown
 dec eax
 sub eax,edx
 jmp .row
.topdown:
 mov eax,edx
.row:
 imul eax,[qd_bmp_stride]
 add eax,[qd_bmp_pixels]
 mov esi,eax
 mov ebx,[qd_bmp_bpp]
 cmp ebx,1
 je .indexed1
 cmp ebx,8
 je .indexed8
 cmp ebx,24
 je .rgb24
 lea esi,[esi+ecx*4]
 jmp .rgb
.rgb24:
 lea eax,[ecx+ecx*2]
 add esi,eax
 jmp .rgb
.indexed1:
 mov eax,ecx
 shr eax,3
 movzx edi,byte [esi+eax]
 and ecx,7
 mov eax,7
 sub eax,ecx
 mov ecx,eax
 shr edi,cl
 and edi,1
 jmp .palette
.indexed8:
 movzx edi,byte [esi+ecx]
.palette:
 cmp edi,[qd_bmp_palette_len]
 jae .white
 mov esi,[qd_bmp_palette]
 lea esi,[esi+edi*4]
.rgb:
 movzx eax,byte [esi]
 movzx edx,byte [esi+1]
 add eax,edx
 movzx edx,byte [esi+2]
 add eax,edx
 xor edx,edx
 mov ebx,3
 div ebx
 cmp eax,[qd_bmp_threshold]
 setb al
 movzx eax,al
 jmp .end
.white:
 xor eax,eax
.end:
 pop edi
 pop esi
 pop edx
 pop ecx
 pop ebx
 ret

qd_getdark:
 call qd_getpixel
 xor eax,[qd_bmp_invert]
 ret

; Locate axis-aligned dark rectangle, supporting inverted images with a quiet border.
qd_find_bbox:
 mov [qd_bmp_invert],0
 xor ebx,ebx
 mov ecx,0
 mov edx,0
 call qd_getpixel
 add ebx,eax
 mov ecx,[qd_bmp_w]
 dec ecx
 mov edx,0
 call qd_getpixel
 add ebx,eax
 mov ecx,0
 mov edx,[qd_bmp_h]
 dec edx
 call qd_getpixel
 add ebx,eax
 mov ecx,[qd_bmp_w]
 dec ecx
 mov edx,[qd_bmp_h]
 dec edx
 call qd_getpixel
 add ebx,eax
 cmp ebx,3
 jb .scan
 mov [qd_bmp_invert],1
.scan:
 mov eax,[qd_bmp_w]
 mov [qd_minx],eax
 mov [qd_maxx],-1
 mov eax,[qd_bmp_h]
 mov [qd_miny],eax
 mov [qd_maxy],-1
 mov [qd_scany],0
.y:
 mov eax,[qd_scany]
 cmp eax,[qd_bmp_h]
 jae .done
 mov [qd_scanx],0
.x:
 mov eax,[qd_scanx]
 cmp eax,[qd_bmp_w]
 jae .next_y
 mov ecx,eax
 mov edx,[qd_scany]
 call qd_getdark
 test eax,eax
 jz .next_x
 mov eax,[qd_scanx]
 cmp eax,[qd_minx]
 jae @f
 mov [qd_minx],eax
@@:
 cmp eax,[qd_maxx]
 jle @f
 mov [qd_maxx],eax
@@:
 mov eax,[qd_scany]
 cmp eax,[qd_miny]
 jae @f
 mov [qd_miny],eax
@@:
 cmp eax,[qd_maxy]
 jle @f
 mov [qd_maxy],eax
@@:
.next_x:
 inc [qd_scanx]
 jmp .x
.next_y:
 inc [qd_scany]
 jmp .y
.done:
 cmp [qd_maxx],0
 jl .fail
 mov eax,[qd_maxx]
 sub eax,[qd_minx]
 inc eax
 mov [qd_bbox_w],eax
 mov ebx,[qd_maxy]
 sub ebx,[qd_miny]
 inc ebx
 mov [qd_bbox_h],ebx
 ; Symbol must be square within 7% (tolerates pixel-grid rounding).
 mov ecx,eax
 sub ecx,ebx
 jns @f
 neg ecx
@@:
 mov edx,eax
 cmp edx,ebx
 jae @f
 mov edx,ebx
@@:
 imul ecx,100
 imul edx,7
 cmp ecx,edx
 ja .fail
 clc
 ret
.fail:
 stc
 ret

; Sample the center of each module in the bounding rectangle.
qd_sample_bbox:
 mov [qd_ty],0
.y:
 mov esi,[qd_ty]
 cmp esi,[qd_side]
 jae .done
 lea eax,[esi*2+1]
 imul eax,[qd_bbox_h]
 xor edx,edx
 mov ecx,[qd_side]
 shl ecx,1
 div ecx
 add eax,[qd_miny]
 mov [qd_imgy],eax
 mov [qd_tx],0
.x:
 mov esi,[qd_tx]
 cmp esi,[qd_side]
 jae .next_y
 lea eax,[esi*2+1]
 imul eax,[qd_bbox_w]
 xor edx,edx
 mov ecx,[qd_side]
 shl ecx,1
 div ecx
 add eax,[qd_minx]
 mov ecx,eax
 mov edx,[qd_imgy]
 call qd_getdark
 mov edx,[qd_ty]
 imul edx,[qd_side]
 add edx,[qd_tx]
 mov [qd_matrix+edx],al
 inc [qd_tx]
 jmp .x
.next_y:
 inc [qd_ty]
 jmp .y
.done:
 mov [qd_source],qd_matrix
 ret

; Normalize rotations/reflections to conventional finder orientation.
; Return EAX=payload length or -1; expects qd_source, qd_side.
qd_try_orientations:
 mov [qd_rotation],0
.next:
 cmp [qd_rotation],8
 jae .fail
 call qd_rotate_matrix
 call qd_finder_score
 cmp eax,130
 jb .rotate_next
 push 8192
 push qd_payload
 push [qd_side]
 push qd_rotated
 call qr_decode
 test eax,eax
 jns .done
.rotate_next:
 inc [qd_rotation]
 jmp .next
.fail:
 mov eax,-1
.done:
 ret

; Orientations: first mirror x for 4..7, then rotate by 0/90/180/270.
qd_rotate_matrix:
 mov [qd_ty],0
.y:
 mov eax,[qd_ty]
 cmp eax,[qd_side]
 jae .done
 mov [qd_tx],0
.x:
 mov eax,[qd_tx]
 cmp eax,[qd_side]
 jae .next_y
 mov eax,[qd_tx]
 mov edx,[qd_ty]
 mov ebx,[qd_rotation]
 test ebx,4
 jz .no_mirror
 mov ecx,[qd_side]
 dec ecx
 sub ecx,eax
 mov eax,ecx
.no_mirror:
 and ebx,3
 cmp ebx,0
 je .src_ok
 cmp ebx,1
 je .rotate1
 cmp ebx,2
 je .rotate2
 ; rotation3 source=(y,n-1-x)
 mov ecx,eax
 mov eax,edx
 mov edx,[qd_side]
 dec edx
 sub edx,ecx
 jmp .src_ok
.rotate1:
 mov ecx,edx
 mov edx,eax
 mov eax,[qd_side]
 dec eax
 sub eax,ecx
 jmp .src_ok
.rotate2:
 mov ecx,[qd_side]
 dec ecx
 sub ecx,eax
 mov eax,ecx
 mov ecx,[qd_side]
 dec ecx
 sub ecx,edx
 mov edx,ecx
.src_ok:
 imul edx,[qd_side]
 add edx,eax
 mov esi,[qd_source]
 mov al,[esi+edx]
 mov edx,[qd_ty]
 imul edx,[qd_side]
 add edx,[qd_tx]
 mov [qd_rotated+edx],al
 inc [qd_tx]
 jmp .x
.next_y:
 inc [qd_ty]
 jmp .y
.done:
 ret

; A finder symbol has 7x7 module pattern: black border, 3x3 black center.
qd_finder_score:
 mov [qd_score],0
 mov [qd_finderid],0
.finder:
 cmp [qd_finderid],3
 jae .done
 mov [qd_finderx],0
 mov [qd_findery],0
 cmp [qd_finderid],1
 jne @f
 mov eax,[qd_side]
 sub eax,7
 mov [qd_finderx],eax
@@:
 cmp [qd_finderid],2
 jne @f
 mov eax,[qd_side]
 sub eax,7
 mov [qd_findery],eax
@@:
 mov [qd_fy],0
.y:
 cmp [qd_fy],7
 jae .next_finder
 mov [qd_fx],0
.x:
 cmp [qd_fx],7
 jae .next_y
 ; expected bit from border or inner 3x3
 xor ebx,ebx
 mov eax,[qd_fx]
 test eax,eax
 jz .expected_black
 cmp eax,6
 je .expected_black
 mov eax,[qd_fy]
 test eax,eax
 jz .expected_black
 cmp eax,6
 je .expected_black
 mov eax,[qd_fx]
 cmp eax,2
 jb .sample
 cmp eax,4
 ja .sample
 mov eax,[qd_fy]
 cmp eax,2
 jb .sample
 cmp eax,4
 ja .sample
.expected_black:
 mov ebx,1
.sample:
 mov edx,[qd_fy]
 add edx,[qd_findery]
 imul edx,[qd_side]
 mov eax,[qd_fx]
 add eax,[qd_finderx]
 add edx,eax
 movzx eax,byte [qd_rotated+edx]
 cmp eax,ebx
 jne .mismatch
 inc [qd_score]
.mismatch:
 inc [qd_fx]
 jmp .x
.next_y:
 inc [qd_fy]
 jmp .y
.next_finder:
 inc [qd_finderid]
 jmp .finder
.done:
 mov eax,[qd_score]
 ret

include 'qr32.inc'

section '.idata' import data readable writeable
library kernel32,'KERNEL32.DLL'
import kernel32,\
  GetCommandLineA,'GetCommandLineA',\
  GetStdHandle,'GetStdHandle',\
  CreateFileA,'CreateFileA',\
  GetFileSize,'GetFileSize',\
  ReadFile,'ReadFile',\
  WriteFile,'WriteFile',\
  CloseHandle,'CloseHandle',\
  GlobalAlloc,'GlobalAlloc',\
  ExitProcess,'ExitProcess'
