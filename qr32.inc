; qr32.inc -- dependency-free QR Code Model 2 decoder, FASM/Win32 x86
; Input: exact unrotated QR module matrix (0 light / nonzero dark).
;   push output_capacity
;   push output_address
;   push side_length                 ; 21, 25, ... 177
;   push byte_matrix_address        ; side_length^2 bytes, row-major
;   call qr_decode
;   EAX = number of decoded payload bytes, or negative status.
; The output is binary, not zero terminated.  qr_eci, qr_sa_* provide metadata.
; No CRT, heap allocation, DLL imports or system calls. Non-reentrant scratch.
;
; Codes: -1 invalid arguments, -2 bad format, -3 uncorrectable RS,
;        -4 unsupported/invalid data segments, -5 output too short,
;        -6 invalid function/codeword layout.
;
; QR block tables originate from QR-Code-generator (Project Nayuki, MIT):
; https://github.com/nayuki/QR-Code-generator
; The implementation below is independent x86 assembly.

section '.qrdata' data readable writeable
align 4
qr_matrix dd 0
qr_n dd 0
qr_version dd 0
qr_output dd 0
qr_output_cap dd 0
qr_output_len dd 0
qr_eci dd 3                       ; default Latin-1 according to ECI assignment
qr_fnc1 dd 0
qr_sa_index dd -1
qr_sa_total dd 0
qr_sa_parity dd 0
qr_pending_percent dd 0
qr_format_a dd 0
qr_format_b dd 0
qr_format_choice dd 0
qr_format_best dd 0
qr_format_score dd 0
qr_format_trial dd 0
qr_mask dd 0
qr_ecc_class dd 0
qr_ecc_len dd 0
qr_num_blocks dd 0
qr_totalwords dd 0
qr_rawbits dd 0
qr_rawpos dd 0
qr_bitvalue dd 0
qr_right dd 0
qr_vert dd 0
qr_x dd 0
qr_y dd 0
qr_j dd 0
qr_i dd 0
qr_k dd 0
qr_ai dd 0
qr_aj dd 0
qr_acount dd 0
qr_astep dd 0
qr_ax dd 0
qr_ay dd 0
qr_sx dd 0
qr_sy dd 0
qr_short_total dd 0
qr_short_data dd 0
qr_num_short dd 0
qr_block_index dd 0
qr_block_len dd 0
qr_all_data_len dd 0
qr_stream_index dd 0
qr_bitpos dd 0
qr_readcount dd 0
qr_mode dd 0
qr_count dd 0
qr_value dd 0
qr_tmp dd 0
qr_tmp2 dd 0
qr_tmp3 dd 0
qr_gfready dd 0
qr_rs_ptr dd 0
qr_rs_n dd 0
qr_rs_ecc dd 0
qr_rs_k dd 0
qr_rs_p dd 0
qr_rs_s dd 0
qr_rs_root dd 0
qr_rs_acc dd 0
qr_rs_L dd 0
qr_rs_m dd 0
qr_rs_b dd 0
qr_rs_d dd 0
qr_rs_nth dd 0
qr_rs_i dd 0
qr_rs_j dd 0
qr_rs_coef dd 0
qr_rs_found dd 0
qr_rs_power dd 0
qr_rs_errroot dd 0
qr_rs_row dd 0
qr_rs_col dd 0
qr_rs_pivot dd 0
qr_rs_factor dd 0
qr_rs_stride dd 32
qr_rs_matrix_index dd 0
qr_alnum db '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:'
qr_ecc_per_block:
 db -1,7,10,15,20,26,18,20,24,30,18,20,24,26,30,22,24,28,30,28,28,28,28,30,30,26,28,30,30,30,30,30,30,30,30,30,30,30,30,30,30
 db -1,10,16,26,18,24,16,18,22,22,26,30,22,22,24,24,28,28,26,26,26,26,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28
 db -1,13,22,18,26,18,24,18,22,20,24,28,26,24,20,30,24,28,28,26,30,28,30,30,30,30,28,30,30,30,30,30,30,30,30,30,30,30,30,30,30
 db -1,17,28,22,16,22,28,26,26,24,28,24,28,22,24,24,30,28,28,26,28,30,24,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30
qr_blocks_per_ecc:
 db -1,1,1,1,1,1,2,2,2,2,4,4,4,4,4,6,6,6,6,7,8,8,9,9,10,12,12,12,13,14,15,16,17,18,19,19,20,21,22,24,25
 db -1,1,1,1,2,2,4,4,4,5,5,5,8,9,9,10,10,11,13,14,16,17,17,18,20,21,23,25,26,28,29,31,33,35,37,38,40,43,45,47,49
 db -1,1,1,2,2,4,4,6,6,8,8,8,10,12,16,12,17,16,18,21,20,23,23,25,27,29,34,34,35,38,40,43,45,48,51,53,56,59,62,65,68
 db -1,1,1,2,4,4,4,5,6,8,8,11,11,16,16,18,16,19,21,25,25,25,34,30,32,35,37,40,42,45,48,51,54,57,60,63,66,70,74,77,81

; --- Ab hier nur noch reservierter Speicher (BSS) ---
qr_align_positions rb 7
qr_function rb 31329
qr_raw rb 3706
qr_blocks rb 20736               ; 81 x 256
qr_data rb 3706
qr_syndrome rb 31
qr_rs_C rb 32
qr_rs_B rb 32
qr_rs_T rb 32
qr_rs_errpos rb 16
qr_rs_powers rb 16
qr_rs_matrix rb 512                ; 16 rows of stride 32
qr_gf_exp rb 512
qr_gf_log rb 256
section '.qrcode' code readable executable

qr_decode:
 push ebp
 mov ebp,esp
 push ebx
 push esi
 push edi
 mov eax,[ebp+8]
 test eax,eax
 jz .invalid
 mov [qr_matrix],eax
 mov eax,[ebp+12]
 mov [qr_n],eax
 cmp eax,21
 jb .invalid
 cmp eax,177
 ja .invalid
 sub eax,17
 test eax,3
 jnz .invalid
 shr eax,2
 mov [qr_version],eax
 mov eax,[ebp+16]
 test eax,eax
 jz .invalid
 mov [qr_output],eax
 mov eax,[ebp+20]
 test eax,eax
 jz .invalid
 mov [qr_output_cap],eax
 mov [qr_output_len],0
 mov [qr_fnc1],0
 mov [qr_eci],3
 mov [qr_sa_index],-1
 mov [qr_sa_total],0
 mov [qr_sa_parity],0
 mov [qr_pending_percent],0
 call qr_read_format
 jc .bad_format
 call qr_prepare_layout
 jc .bad_layout
 call qr_read_codewords
 jc .bad_layout
 call qr_reconstruct_data
 jc .bad_rs
 call qr_parse_segments
 jc .bad_segments
 mov eax,[qr_output_len]
 jmp .finish
.invalid:
 mov eax,-1
 jmp .finish
.bad_format:
 mov eax,-2
 jmp .finish
.bad_rs:
 mov eax,-3
 jmp .finish
.bad_segments:
 ; qr_parse_segments sets -5 for insufficient capacity
 cmp eax,-5
 je .finish
 mov eax,-4
 jmp .finish
.bad_layout:
 mov eax,-6
.finish:
 pop edi
 pop esi
 pop ebx
 pop ebp
 ret 16

; Read QR pixel: ECX=x EDX=y, EAX=0 or 1; preserve ECX, EDX.
qr_getmod:
 mov eax,[qr_n]
 imul eax,edx
 add eax,ecx
 add eax,[qr_matrix]
 movzx eax,byte [eax]
 test eax,eax
 setnz al
 movzx eax,al
 ret

; Format data (two 15-bit copies) checked against all BCH(15,5) codewords.
qr_read_format:
 mov [qr_format_a],0
 mov [qr_format_b],0
 xor esi,esi
.a_top:
 cmp esi,6
 jae .a_special
 mov ecx,8
 mov edx,esi
 call qr_getmod
 test eax,eax
 jz .a_next
 bts dword [qr_format_a],esi
.a_next:
 inc esi
 jmp .a_top
.a_special:
 mov ecx,8
 mov edx,7
 call qr_getmod
 test eax,eax
 jz @f
 bts dword [qr_format_a],6
@@:
 mov ecx,8
 mov edx,8
 call qr_getmod
 test eax,eax
 jz @f
 bts dword [qr_format_a],7
@@:
 mov ecx,7
 mov edx,8
 call qr_getmod
 test eax,eax
 jz @f
 bts dword [qr_format_a],8
@@:
 mov esi,9
.a_left:
 cmp esi,15
 jae .b_right
 mov ecx,14
 sub ecx,esi
 mov edx,8
 call qr_getmod
 test eax,eax
 jz .a_left_next
 bts dword [qr_format_a],esi
.a_left_next:
 inc esi
 jmp .a_left
.b_right:
 xor esi,esi
.b_loop1:
 cmp esi,8
 jae .b_bottom
 mov ecx,[qr_n]
 dec ecx
 sub ecx,esi
 mov edx,8
 call qr_getmod
 test eax,eax
 jz .b_next1
 bts dword [qr_format_b],esi
.b_next1:
 inc esi
 jmp .b_loop1
.b_bottom:
 mov esi,8
.b_loop2:
 cmp esi,15
 jae .compare
 mov ecx,8
 mov edx,[qr_n]
 sub edx,15
 add edx,esi
 call qr_getmod
 test eax,eax
 jz .b_next2
 bts dword [qr_format_b],esi
.b_next2:
 inc esi
 jmp .b_loop2
.compare:
 mov [qr_format_best],4
 mov [qr_format_score],0FFFFFFFFh
 xor esi,esi
.try:
 cmp esi,32
 jae .selected
 ; BCH remainder: shift 5-bit data through 10 polynomial iterations.
 mov eax,esi
 mov ecx,10
.bch:
 mov edx,eax
 shr edx,9
 and edx,1
 shl eax,1
 test edx,edx
 jz @f
 xor eax,537h
@@:
 dec ecx
 jnz .bch
 and eax,3FFh
 mov edx,esi
 shl edx,10
 or eax,edx
 xor eax,5412h
 mov ebx,eax
 xor eax,[qr_format_a]
 call qr_popcount15
 mov [qr_tmp],eax
 mov eax,ebx
 xor eax,[qr_format_b]
 call qr_popcount15
 mov edx,[qr_tmp]
 add edx,eax            ; tie breaker (total Hamming distance)
 cmp eax,[qr_tmp]
 jae @f
 mov [qr_tmp],eax
@@:
 cmp [qr_tmp],3
 ja .next
 mov eax,[qr_tmp]
 cmp eax,[qr_format_best]
 ja .next
 jb .accept
 cmp edx,[qr_format_score]
 jae .next
.accept:
 mov [qr_format_best],eax
 mov [qr_format_score],edx
 mov [qr_format_choice],esi
.next:
 inc esi
 jmp .try
.selected:
 cmp [qr_format_best],3
 ja .fail
 mov eax,[qr_format_choice]
 mov ecx,eax
 and ecx,7
 mov [qr_mask],ecx
 shr eax,3
 ; encoded ECC bits: L=1, M=0, Q=3, H=2
 cmp eax,1
 je .low
 cmp eax,0
 je .medium
 cmp eax,3
 je .quartile
 mov [qr_ecc_class],3
 jmp .ok
.low:
 mov [qr_ecc_class],0
 jmp .ok
.medium:
 mov [qr_ecc_class],1
 jmp .ok
.quartile:
 mov [qr_ecc_class],2
.ok:
 clc
 ret
.fail:
 stc
 ret

qr_popcount15:
 push ecx
 mov ecx,15
 xor edx,edx
.loop:
 shr eax,1
 adc edx,0
 dec ecx
 jnz .loop
 mov eax,edx
 pop ecx
 ret

; Reserve all non-data modules (finder, separator, timing, align, format,
; version metadata). qr_mark_func clips coordinates to [0,n).
qr_prepare_layout:
 mov edi,qr_function
 mov ecx,31329
 xor eax,eax
 rep stosb
 xor esi,esi
.timing:
 cmp esi,[qr_n]
 jae .finders
 mov ecx,6
 mov edx,esi
 call qr_mark_func
 mov ecx,esi
 mov edx,6
 call qr_mark_func
 inc esi
 jmp .timing
.finders:
 mov [qr_ax],3
 mov [qr_ay],3
 call qr_mark_finder
 mov eax,[qr_n]
 sub eax,4
 mov [qr_ax],eax
 mov [qr_ay],3
 call qr_mark_finder
 mov eax,[qr_n]
 sub eax,4
 mov [qr_ay],eax
 mov [qr_ax],3
 call qr_mark_finder
 ; version 1 has no alignment symbols
 cmp [qr_version],1
 je .formats
 mov eax,[qr_version]
 xor edx,edx
 mov ecx,7
 div ecx
 add eax,2
 mov [qr_acount],eax
 ; step = ((version*8 + nAlign*3 +5) / (nAlign*4 - 4))*2
 cmp [qr_version],32
 jne .calc_step
 mov [qr_astep],26
 jmp .align_positions
.calc_step:
 mov eax,[qr_version]
 shl eax,3
 mov ecx,[qr_acount]
 lea eax,[eax+ecx*2+5]
 add eax,ecx         ; 8*v + 3*n + 5
 lea ecx,[ecx*4-4]
 xor edx,edx
 div ecx
 shl eax,1
 mov [qr_astep],eax
.align_positions:
 mov byte [qr_align_positions],6
 mov esi,[qr_acount]
 dec esi
 mov eax,[qr_n]
 sub eax,7
.ap_loop:
 cmp esi,0
 jle .align_pairs
 mov [qr_align_positions+esi],al
 sub eax,[qr_astep]
 dec esi
 jmp .ap_loop
.align_pairs:
 xor esi,esi
.ap_y:
 cmp esi,[qr_acount]
 jae .formats
 xor ebx,ebx
.ap_x:
 cmp ebx,[qr_acount]
 jae .ap_next_y
 ; skip top-left, top-right and bottom-left finder area
 cmp esi,0
 jne .ap_not_top
 cmp ebx,0
 je .ap_next_x
 mov eax,[qr_acount]
 dec eax
 cmp ebx,eax
 je .ap_next_x
.ap_not_top:
 cmp ebx,0
 jne .ap_draw
 mov eax,[qr_acount]
 dec eax
 cmp esi,eax
 je .ap_next_x
.ap_draw:
 movzx eax,byte [qr_align_positions+ebx]
 mov [qr_ax],eax
 movzx eax,byte [qr_align_positions+esi]
 mov [qr_ay],eax
 call qr_mark_align
.ap_next_x:
 inc ebx
 jmp .ap_x
.ap_next_y:
 inc esi
 jmp .ap_y
.formats:
 xor esi,esi
.fmt_loop:
 cmp esi,9
 jae .fmt_second
 mov ecx,8
 mov edx,esi
 call qr_mark_func
 mov ecx,esi
 mov edx,8
 call qr_mark_func
 inc esi
 jmp .fmt_loop
.fmt_second:
 xor esi,esi
.fmt_loop2:
 cmp esi,8
 jae .version
 mov ecx,[qr_n]
 dec ecx
 sub ecx,esi
 mov edx,8
 call qr_mark_func
 mov ecx,8
 mov edx,[qr_n]
 dec edx
 sub edx,esi
 call qr_mark_func
 inc esi
 jmp .fmt_loop2
.version:
 cmp [qr_version],7
 jb .totals
 xor esi,esi
.v_y:
 cmp esi,6
 jae .totals
 xor ebx,ebx
.v_x:
 cmp ebx,3
 jae .v_next_y
 mov ecx,[qr_n]
 sub ecx,11
 add ecx,ebx
 mov edx,esi
 call qr_mark_func
 mov ecx,esi
 mov edx,[qr_n]
 sub edx,11
 add edx,ebx
 call qr_mark_func
 inc ebx
 jmp .v_x
.v_next_y:
 inc esi
 jmp .v_y
.totals:
 ; Total non-function modules incl. remainder bits.
 mov eax,[qr_version]
 shl eax,4
 add eax,128
 imul eax,[qr_version]
 add eax,64
 mov [qr_rawbits],eax
 cmp [qr_version],2
 jb .raw_word_count
 mov eax,[qr_version]
 xor edx,edx
 mov ecx,7
 div ecx
 add eax,2
 mov ecx,eax
 imul eax,25
 sub eax,10
 imul eax,ecx
 sub eax,55
 sub [qr_rawbits],eax
 cmp [qr_version],7
 jb .raw_word_count
 sub [qr_rawbits],36
.raw_word_count:
 mov eax,[qr_rawbits]
 shr eax,3
 mov [qr_totalwords],eax
 cmp eax,3706
 ja .bad
 ; ECC tables are indexed by ECC class and version (41 elements/class).
 mov eax,[qr_ecc_class]
 imul eax,41
 add eax,[qr_version]
 movzx ecx,byte [qr_ecc_per_block+eax]
 mov [qr_ecc_len],ecx
 movzx ecx,byte [qr_blocks_per_ecc+eax]
 mov [qr_num_blocks],ecx
 cmp ecx,1
 jb .bad
 cmp ecx,81
 ja .bad
 clc
 ret
.bad:
 stc
 ret

qr_mark_finder:
 mov [qr_sy],-4
.y:
 cmp [qr_sy],4
 jg .done
 mov [qr_sx],-4
.x:
 cmp [qr_sx],4
 jg .next_y
 mov ecx,[qr_ax]
 add ecx,[qr_sx]
 mov edx,[qr_ay]
 add edx,[qr_sy]
 call qr_mark_func
 inc [qr_sx]
 jmp .x
.next_y:
 inc [qr_sy]
 jmp .y
.done:
 ret

qr_mark_align:
 mov [qr_sy],-2
.y:
 cmp [qr_sy],2
 jg .done
 mov [qr_sx],-2
.x:
 cmp [qr_sx],2
 jg .next_y
 mov ecx,[qr_ax]
 add ecx,[qr_sx]
 mov edx,[qr_ay]
 add edx,[qr_sy]
 call qr_mark_func
 inc [qr_sx]
 jmp .x
.next_y:
 inc [qr_sy]
 jmp .y
.done:
 ret

qr_mark_func:
 test ecx,ecx
 js .done
 test edx,edx
 js .done
 cmp ecx,[qr_n]
 jae .done
 cmp edx,[qr_n]
 jae .done
 mov eax,edx
 imul eax,[qr_n]
 add eax,ecx
 mov byte [qr_function+eax],1
.done:
 ret

; Get mask value for coordinates ECX=x, EDX=y (return EAX=0 or 1).
qr_mask_bit:
 push ebx
 push ecx
 push edx
 mov ebx,[qr_mask]
 cmp ebx,0
 je .m0
 cmp ebx,1
 je .m1
 cmp ebx,2
 je .m2
 cmp ebx,3
 je .m3
 cmp ebx,4
 je .m4
 cmp ebx,5
 je .m5
 cmp ebx,6
 je .m6
 ; m7: ((x+y)%2 + (x*y)%3)%2 == 0
 mov ebx,ecx
 add ebx,edx
 and ebx,1
 mov eax,ecx
 imul eax,edx
 xor edx,edx
 mov ecx,3
 div ecx
 add ebx,edx
 and ebx,1
 jmp .parity
.m0:
 lea eax,[ecx+edx]
 and eax,1
 xor eax,1
 jmp .end
.m1:
 mov eax,edx
 and eax,1
 xor eax,1
 jmp .end
.m2:
 mov eax,ecx
 xor edx,edx
 mov ecx,3
 div ecx
 test edx,edx
 setz al
 movzx eax,al
 jmp .end
.m3:
 lea eax,[ecx+edx]
 xor edx,edx
 mov ecx,3
 div ecx
 test edx,edx
 setz al
 movzx eax,al
 jmp .end
.m4:
 mov eax,ecx
 xor edx,edx
 mov ebx,3
 div ebx
 mov ecx,eax
 mov eax,[esp]
 shr eax,1
 add eax,ecx
 and eax,1
 xor eax,1
 jmp .end
.m5:
 mov eax,ecx
 imul eax,edx
 mov ebx,eax
 and ebx,1
 xor edx,edx
 mov ecx,3
 div ecx
 add ebx,edx
 test ebx,ebx
 setz al
 movzx eax,al
 jmp .end
.m6:
 mov eax,ecx
 imul eax,edx
 mov ebx,eax
 and ebx,1
 xor edx,edx
 mov ecx,3
 div ecx
 add ebx,edx
 and ebx,1
.parity:
 mov eax,ebx
 xor eax,1
.end:
 pop edx
 pop ecx
 pop ebx
 ret

; Read unmasked interleaved codewords (skip remainder bits).
qr_read_codewords:
 mov [qr_rawpos],0
 mov [qr_bitvalue],0
 mov eax,[qr_n]
 dec eax
 mov [qr_right],eax
.pair:
 cmp [qr_right],1
 jl .end
 cmp [qr_right],6
 jne .normal
 mov [qr_right],5
.normal:
 mov [qr_vert],0
.row:
 mov eax,[qr_vert]
 cmp eax,[qr_n]
 jae .nextpair
 mov eax,[qr_right]
 inc eax
 and eax,2
 jnz .down
 mov eax,[qr_n]
 dec eax
 sub eax,[qr_vert]
 jmp .y_ready
.down:
 mov eax,[qr_vert]
.y_ready:
 mov [qr_y],eax
 mov [qr_j],0
.cell:
 cmp [qr_j],2
 jae .nextrow
 mov eax,[qr_right]
 sub eax,[qr_j]
 mov [qr_x],eax
 mov edx,[qr_y]
 imul edx,[qr_n]
 add edx,eax
 cmp byte [qr_function+edx],0
 jne .nextcell
 ; qr_rawpos tracks bits; skip remainder once totalwords*8 read.
 mov eax,[qr_totalwords]
 shl eax,3
 cmp [qr_rawpos],eax
 jae .nextcell
 mov ecx,[qr_x]
 mov edx,[qr_y]
 call qr_getmod
 mov [qr_tmp],eax
 mov ecx,[qr_x]
 mov edx,[qr_y]
 call qr_mask_bit
 xor eax,[qr_tmp]
 mov ecx,[qr_bitvalue]
 shl ecx,1
 or ecx,eax
 mov [qr_bitvalue],ecx
 inc [qr_rawpos]
 mov eax,[qr_rawpos]
 and eax,7
 jnz .nextcell
 mov eax,[qr_rawpos]
 shr eax,3
 dec eax
 mov edx,[qr_bitvalue]
 mov byte [qr_raw+eax],dl
 mov [qr_bitvalue],0
.nextcell:
 inc [qr_j]
 jmp .cell
.nextrow:
 inc [qr_vert]
 jmp .row
.nextpair:
 sub [qr_right],2
 jmp .pair
.end:
 mov eax,[qr_totalwords]
 shl eax,3
 cmp [qr_rawpos],eax
 jne .bad
 clc
 ret
.bad:
 stc
 ret

qr_init_gf:
 cmp [qr_gfready],0
 jne .done
 mov esi,0
 mov ebx,1
.loop:
 cmp esi,255
 jae .repeat
 mov byte [qr_gf_exp+esi],bl
 mov eax,esi
 mov byte [qr_gf_log+ebx],al
 shl ebx,1
 cmp ebx,256
 jb @f
 xor ebx,11Dh
@@:
 inc esi
 jmp .loop
.repeat:
 mov esi,255
.copy:
 cmp esi,512
 jae .ready
 mov eax,esi
 sub eax,255
 mov al,[qr_gf_exp+eax]
 mov [qr_gf_exp+esi],al
 inc esi
 jmp .copy
.ready:
 mov [qr_gfready],1
.done:
 ret

; GF(256) multiply: AL=a, BL=b -> AL=a*b (preserves EBX/ECX/EDX).
qr_gfmul:
 push edx
 test al,al
 jz .zero
 test bl,bl
 jz .zero
 movzx edx,al
 movzx edx,byte [qr_gf_log+edx]
 movzx eax,bl
 movzx eax,byte [qr_gf_log+eax]
 add eax,edx
 movzx eax,byte [qr_gf_exp+eax]
 pop edx
 ret
.zero:
 xor eax,eax
 pop edx
 ret

; GF(256) divide: AL=a, BL=b !=0 -> AL=a/b (preserves EBX/EDX).
qr_gfdiv:
 push edx
 test al,al
 jz .zero
 movzx edx,al
 movzx edx,byte [qr_gf_log+edx]
 movzx eax,bl
 movzx eax,byte [qr_gf_log+eax]
 sub edx,eax
 jns @f
 add edx,255
@@:
 movzx eax,byte [qr_gf_exp+edx]
 pop edx
 ret
.zero:
 xor eax,eax
 pop edx
 ret

; The interleaved data consists of short and long RS blocks.
qr_reconstruct_data:
 call qr_init_gf
 mov eax,[qr_totalwords]
 xor edx,edx
 div [qr_num_blocks]
 mov [qr_short_total],eax
 mov ecx,[qr_num_blocks]
 sub ecx,edx
 mov [qr_num_short],ecx
 mov ebx,eax
 sub ebx,[qr_ecc_len]
 mov [qr_short_data],ebx
 cmp ebx,0
 jle .bad
 mov eax,[qr_ecc_len]
 imul eax,[qr_num_blocks]
 mov edx,[qr_totalwords]
 sub edx,eax
 cmp edx,0
 jle .bad
 mov [qr_all_data_len],edx
 ; Interleaved order: column i over blocks; skip the last data
 ; word on the short blocks, not a parity word.
 mov [qr_stream_index],0
 mov [qr_i],0
.row:
 mov eax,[qr_i]
 cmp eax,[qr_short_total]
 ja .after_deinterleave
 mov [qr_block_index],0
.col:
 mov ebx,[qr_block_index]
 cmp ebx,[qr_num_blocks]
 jae .nextrow
 mov eax,[qr_i]
 cmp eax,[qr_short_data]
 jne .copy
 cmp ebx,[qr_num_short]
 jb .nextblock
.copy:
 mov eax,[qr_i]
 cmp ebx,[qr_num_short]
 jae .offset_ready
 cmp eax,[qr_short_data]
 jbe .offset_ready
 dec eax
.offset_ready:
 mov edx,ebx
 shl edx,8
 add edx,eax
 mov eax,[qr_stream_index]
 cmp eax,[qr_totalwords]
 jae .bad
 mov cl,[qr_raw+eax]
 mov [qr_blocks+edx],cl
 inc [qr_stream_index]
.nextblock:
 inc [qr_block_index]
 jmp .col
.nextrow:
 inc [qr_i]
 jmp .row
.after_deinterleave:
 mov eax,[qr_stream_index]
 cmp eax,[qr_totalwords]
 jne .bad
 mov [qr_stream_index],0
 mov [qr_block_index],0
.correct:
 mov esi,[qr_block_index]
 cmp esi,[qr_num_blocks]
 jae .done
 mov eax,[qr_short_total]
 cmp esi,[qr_num_short]
 jb @f
 inc eax
@@:
 mov [qr_block_len],eax
 mov [qr_rs_n],eax
 mov eax,[qr_ecc_len]
 mov [qr_rs_ecc],eax
 mov eax,esi
 shl eax,8
 add eax,qr_blocks
 mov [qr_rs_ptr],eax
 call qr_rs_correct
 jc .bad
 ; Reassemble data sections in original block order.
 mov esi,[qr_rs_ptr]
 mov edi,qr_data
 add edi,[qr_stream_index]
 mov ecx,[qr_block_len]
 sub ecx,[qr_ecc_len]
 add [qr_stream_index],ecx
 rep movsb
 inc [qr_block_index]
 jmp .correct
.done:
 mov eax,[qr_stream_index]
 cmp eax,[qr_all_data_len]
 jne .bad
 clc
 ret
.bad:
 stc
 ret

; Compute QR Reed-Solomon syndromes S_j = P(alpha^j).
; CF=1 if any syndrome nonzero; CF=0 iff all zero.
qr_rs_syndromes:
 mov [qr_rs_k],0
 mov [qr_rs_found],0
.kloop:
 mov esi,[qr_rs_k]
 cmp esi,[qr_rs_ecc]
 jae .done
 movzx eax,byte [qr_gf_exp+esi]
 mov [qr_rs_root],eax
 mov [qr_rs_acc],0
 mov [qr_rs_p],0
.ploop:
 mov esi,[qr_rs_p]
 cmp esi,[qr_rs_n]
 jae .store
 mov eax,[qr_rs_acc]
 mov ebx,[qr_rs_root]
 call qr_gfmul
 mov edx,[qr_rs_ptr]
 mov ecx,[qr_rs_p]
 xor al,[edx+ecx]
 movzx eax,al
 mov [qr_rs_acc],eax
 inc [qr_rs_p]
 jmp .ploop
.store:
 mov eax,[qr_rs_acc]
 mov edx,[qr_rs_k]
 mov [qr_syndrome+edx],al
 test al,al
 jz @f
 mov [qr_rs_found],1
@@:
 inc [qr_rs_k]
 jmp .kloop
.done:
 cmp [qr_rs_found],0
 je .zero
 stc
 ret
.zero:
 clc
 ret

qr_rs_correct:
 call qr_rs_syndromes
 jnc .good
 mov edi,qr_rs_C
 mov ecx,32*3
 xor eax,eax
 rep stosb
 mov byte [qr_rs_C],1
 mov byte [qr_rs_B],1
 mov [qr_rs_L],0
 mov [qr_rs_m],1
 mov [qr_rs_b],1
 mov [qr_rs_nth],0
.bmstep:
 mov esi,[qr_rs_nth]
 cmp esi,[qr_rs_ecc]
 jae .bm_done
 movzx eax,byte [qr_syndrome+esi]
 mov [qr_rs_d],eax
 mov [qr_rs_i],1
.delta:
 mov ecx,[qr_rs_i]
 cmp ecx,[qr_rs_L]
 ja .gotdelta
 movzx eax,byte [qr_rs_C+ecx]
 mov ebx,[qr_rs_nth]
 sub ebx,ecx
 movzx ebx,byte [qr_syndrome+ebx]
 call qr_gfmul
 xor [qr_rs_d],eax
 inc [qr_rs_i]
 jmp .delta
.gotdelta:
 cmp [qr_rs_d],0
 jne .update
 inc [qr_rs_m]
 jmp .bmnext
.update:
 mov esi,qr_rs_C
 mov edi,qr_rs_T
 mov ecx,32
 rep movsb
 mov eax,[qr_rs_d]
 mov ebx,[qr_rs_b]
 call qr_gfdiv
 mov [qr_rs_coef],eax
 mov [qr_rs_i],0
.update_poly:
 mov ecx,[qr_rs_i]
 add ecx,[qr_rs_m]
 cmp ecx,[qr_rs_ecc]
 ja .update_decide
 mov edx,ecx
 sub edx,[qr_rs_m]
 movzx eax,byte [qr_rs_B+edx]
 mov ebx,[qr_rs_coef]
 call qr_gfmul
 xor byte [qr_rs_C+ecx],al
 inc [qr_rs_i]
 jmp .update_poly
.update_decide:
 mov eax,[qr_rs_L]
 shl eax,1
 cmp eax,[qr_rs_nth]
 ja .advance_m
 mov eax,[qr_rs_nth]
 inc eax
 sub eax,[qr_rs_L]
 mov [qr_rs_L],eax
 mov esi,qr_rs_T
 mov edi,qr_rs_B
 mov ecx,32
 rep movsb
 mov eax,[qr_rs_d]
 mov [qr_rs_b],eax
 mov [qr_rs_m],1
 jmp .bmnext
.advance_m:
 inc [qr_rs_m]
.bmnext:
 inc [qr_rs_nth]
 jmp .bmstep
.bm_done:
 mov eax,[qr_rs_L]
 test eax,eax
 jz .bad
 mov edx,[qr_rs_ecc]
 shr edx,1
 cmp eax,edx
 ja .bad
 ; Locate errors with Chien search. Error at position p has
 ; locator root alpha^(-(n-1-p)).
 mov [qr_rs_found],0
 mov [qr_rs_p],0
.chien:
 mov esi,[qr_rs_p]
 cmp esi,[qr_rs_n]
 jae .located
 mov edx,[qr_rs_n]
 dec edx
 sub edx,esi
 mov [qr_rs_power],edx
 mov eax,255
 sub eax,edx
 movzx eax,byte [qr_gf_exp+eax]
 mov [qr_rs_errroot],eax
 mov ecx,[qr_rs_L]
 movzx eax,byte [qr_rs_C+ecx]
 mov [qr_rs_acc],eax
.chien_eval:
 dec ecx
 js .chien_test
 mov [qr_rs_j],ecx
 mov eax,[qr_rs_acc]
 mov ebx,[qr_rs_errroot]
 call qr_gfmul
 mov ecx,[qr_rs_j]
 xor al,[qr_rs_C+ecx]
 movzx eax,al
 mov [qr_rs_acc],eax
 jmp .chien_eval
.chien_test:
 cmp [qr_rs_acc],0
 jne .chien_next
 mov esi,[qr_rs_found]
 cmp esi,[qr_rs_L]
 jae .bad
 mov eax,[qr_rs_p]
 mov [qr_rs_errpos+esi],al
 mov eax,[qr_rs_power]
 mov [qr_rs_powers+esi],al
 inc [qr_rs_found]
.chien_next:
 inc [qr_rs_p]
 jmp .chien
.located:
 mov eax,[qr_rs_found]
 cmp eax,[qr_rs_L]
 jne .bad
 ; Solve the Vandermonde syndrome system using GF(256) elimination.
 ; Matrix [equation][variable], 16 rows x 32 bytes per row.
 mov edi,qr_rs_matrix
 mov ecx,512
 xor eax,eax
 rep stosb
 mov [qr_rs_row],0
.build_row:
 mov esi,[qr_rs_row]
 cmp esi,[qr_rs_L]
 jae .gauss_init
 mov [qr_rs_col],0
.build_col:
 mov ebx,[qr_rs_col]
 cmp ebx,[qr_rs_L]
 jae .build_rhs
 movzx eax,byte [qr_rs_powers+ebx]
 imul eax,esi
 xor edx,edx
 mov ecx,255
 div ecx
 movzx eax,byte [qr_gf_exp+edx]
 mov edx,esi
 shl edx,5
 add edx,ebx
 mov [qr_rs_matrix+edx],al
 inc [qr_rs_col]
 jmp .build_col
.build_rhs:
 mov edx,esi
 shl edx,5
 add edx,[qr_rs_L]
 mov al,[qr_syndrome+esi]
 mov [qr_rs_matrix+edx],al
 inc [qr_rs_row]
 jmp .build_row
.gauss_init:
 mov [qr_rs_col],0
.gauss_col:
 mov esi,[qr_rs_col]
 cmp esi,[qr_rs_L]
 jae .apply
 ; pivot: find row >= col whose corresponding column is non-zero
 mov [qr_rs_pivot],esi
.find_pivot:
 mov ebx,[qr_rs_pivot]
 cmp ebx,[qr_rs_L]
 jae .bad
 mov edx,ebx
 shl edx,5
 add edx,esi
 cmp byte [qr_rs_matrix+edx],0
 jne .pivot_found
 inc [qr_rs_pivot]
 jmp .find_pivot
.pivot_found:
 cmp ebx,esi
 je .normalise
 ; swap whole rows including RHS
 mov [qr_rs_j],0
.swap:
 mov ecx,[qr_rs_j]
 cmp ecx,[qr_rs_L]
 ja .normalise
 mov edx,esi
 shl edx,5
 add edx,ecx
 mov eax,ebx
 shl eax,5
 add eax,ecx
 mov cl,[qr_rs_matrix+edx]
 mov ch,[qr_rs_matrix+eax]
 mov [qr_rs_matrix+edx],ch
 mov [qr_rs_matrix+eax],cl
 inc [qr_rs_j]
 jmp .swap
.normalise:
 mov eax,esi
 shl eax,5
 add eax,esi
 movzx ebx,byte [qr_rs_matrix+eax]
 ; inverse = 1/pivot
 mov eax,1
 call qr_gfdiv
 mov [qr_rs_coef],eax
 mov [qr_rs_j],esi
.norm_elem:
 mov ecx,[qr_rs_j]
 cmp ecx,[qr_rs_L]
 ja .eliminate
 mov edx,esi
 shl edx,5
 add edx,ecx
 movzx eax,byte [qr_rs_matrix+edx]
 mov ebx,[qr_rs_coef]
 call qr_gfmul
 mov ecx,[qr_rs_j]
 mov edx,esi
 shl edx,5
 add edx,ecx
 mov [qr_rs_matrix+edx],al
 inc [qr_rs_j]
 jmp .norm_elem
.eliminate:
 mov [qr_rs_row],0
.elim_row:
 mov eax,[qr_rs_row]
 cmp eax,[qr_rs_L]
 jae .next_gauss_col
 cmp eax,[qr_rs_col]
 je .skip_row
 mov edx,eax
 shl edx,5
 add edx,[qr_rs_col]
 movzx eax,byte [qr_rs_matrix+edx]
 mov [qr_rs_factor],eax
 test eax,eax
 jz .skip_row
 mov eax,[qr_rs_col]
 mov [qr_rs_j],eax
.elim_elem:
 mov ecx,[qr_rs_j]
 cmp ecx,[qr_rs_L]
 ja .skip_row
 mov edx,[qr_rs_col]
 shl edx,5
 add edx,ecx
 movzx eax,byte [qr_rs_matrix+edx]
 mov ebx,[qr_rs_factor]
 call qr_gfmul
 mov edx,[qr_rs_row]
 shl edx,5
 add edx,[qr_rs_j]
 xor [qr_rs_matrix+edx],al
 inc [qr_rs_j]
 jmp .elim_elem
.skip_row:
 inc [qr_rs_row]
 jmp .elim_row
.next_gauss_col:
 inc [qr_rs_col]
 jmp .gauss_col
.apply:
 mov [qr_rs_i],0
.apply_loop:
 mov esi,[qr_rs_i]
 cmp esi,[qr_rs_L]
 jae .verify
 movzx edx,byte [qr_rs_errpos+esi]
 add edx,[qr_rs_ptr]
 mov eax,esi
 shl eax,5
 add eax,[qr_rs_L]
 mov al,[qr_rs_matrix+eax]
 xor [edx],al
 inc [qr_rs_i]
 jmp .apply_loop
.verify:
 call qr_rs_syndromes
 jc .bad
.good:
 clc
 ret
.bad:
 stc
 ret

; Read ECX most-significant-first bits from corrected QR data into EAX.
; CF=1 if truncated. Clobbers ECX, EDX, ESI, EBX.
qr_readbits:
 cmp ecx,0
 jle .invalid
 cmp ecx,24
 ja .invalid
 mov [qr_readcount],ecx
 mov eax,[qr_bitpos]
 add eax,ecx
 mov ebx,[qr_all_data_len]
 shl ebx,3
 cmp eax,ebx
 ja .invalid
 xor eax,eax
.loop:
 mov ebx,[qr_bitpos]
 mov edx,ebx
 shr ebx,3
 movzx esi,byte [qr_data+ebx]
 and edx,7
 mov ecx,7
 sub ecx,edx
 shr esi,cl
 and esi,1
 shl eax,1
 or eax,esi
 inc [qr_bitpos]
 dec [qr_readcount]
 jnz .loop
 clc
 ret
.invalid:
 stc
 ret

qr_emit:
 push edx
 mov edx,[qr_output_len]
 cmp edx,[qr_output_cap]
 jae .full
 add edx,[qr_output]
 mov [edx],al
 inc [qr_output_len]
 pop edx
 clc
 ret
.full:
 pop edx
 stc
 ret

; Alphanumeric FNC1: single '%' means ASCII GS; doubled '%%' means '%'.
qr_emit_alnum:
 cmp [qr_fnc1],0
 je qr_emit
 cmp [qr_pending_percent],0
 je .new
 mov [qr_pending_percent],0
 cmp al,'%'
 je qr_emit
 push eax
 mov al,1Dh
 call qr_emit
 pop eax
 jc .full
.new:
 cmp al,'%'
 jne qr_emit
 mov [qr_pending_percent],1
 clc
 ret
.full:
 stc
 ret

qr_flush_alnum:
 cmp [qr_pending_percent],0
 je .success
 mov [qr_pending_percent],0
 mov al,1Dh
 call qr_emit
 ret
.success:
 clc
 ret

; Decode segments and concatenate payload as binary bytes.
; Byte segments preserve original bytes; qr_eci reports last active ECI ID.
; Kanji/Hanzi segments are emitted as Shift-JIS/GB2312 byte pairs.
qr_parse_segments:
 mov [qr_bitpos],0
.next_mode:
 ; A truncated mode field at the end is an implicit terminator.
 mov eax,[qr_all_data_len]
 shl eax,3
 sub eax,[qr_bitpos]
 cmp eax,4
 jb .done
 mov ecx,4
 call qr_readbits
 jc .bad
 mov [qr_mode],eax
 test eax,eax
 jz .done
 cmp eax,1
 je .count_field
 cmp eax,2
 je .count_field
 cmp eax,4
 je .count_field
 cmp eax,8
 je .count_field
 cmp eax,13
 je .hanzi_subset
 cmp eax,3
 je .structured_append
 cmp eax,5
 je .fnc1_first
 cmp eax,9
 je .fnc1_second
 cmp eax,7
 je .eci
 jmp .bad
.fnc1_first:
 mov [qr_fnc1],1
 jmp .next_mode
.fnc1_second:
 mov ecx,8
 call qr_readbits
 jc .bad
 mov [qr_fnc1],1
 jmp .next_mode
.structured_append:
 mov ecx,4
 call qr_readbits
 jc .bad
 mov [qr_sa_index],eax
 mov ecx,4
 call qr_readbits
 jc .bad
 inc eax
 mov [qr_sa_total],eax
 mov ecx,8
 call qr_readbits
 jc .bad
 mov [qr_sa_parity],eax
 jmp .next_mode
.eci:
 mov ecx,8
 call qr_readbits
 jc .bad
 mov [qr_value],eax
 test al,80h
 jz .eci_one
 test al,40h
 jnz .eci_three_check
 and eax,3Fh
 shl eax,8
 mov [qr_value],eax
 mov ecx,8
 call qr_readbits
 jc .bad
 or eax,[qr_value]
 jmp .eci_set
.eci_three_check:
 test al,20h
 jnz .bad
 and eax,1Fh
 shl eax,16
 mov [qr_value],eax
 mov ecx,16
 call qr_readbits
 jc .bad
 or eax,[qr_value]
 jmp .eci_set
.eci_one:
 and eax,7Fh
.eci_set:
 mov [qr_eci],eax
 jmp .next_mode
.hanzi_subset:
 mov ecx,4
 call qr_readbits
 jc .bad
 cmp eax,1
 jne .bad
.count_field:
 mov eax,[qr_version]
 mov ebx,0
 cmp eax,9
 jbe .count_width
 mov ebx,1
 cmp eax,26
 jbe .count_width
 mov ebx,2
.count_width:
 mov eax,[qr_mode]
 cmp eax,1
 jne .not_numeric
 mov ecx,10
 jmp .count_adjust
.not_numeric:
 cmp eax,2
 jne .not_alnum
 mov ecx,9
 jmp .count_adjust
.not_alnum:
 cmp eax,4
 jne .wide_mode
 mov ecx,8
 test ebx,ebx
 jz .count_read
 mov ecx,16
 jmp .count_read
.wide_mode:
 mov ecx,8
.count_adjust:
 mov eax,ebx
 shl eax,1
 add ecx,eax
.count_read:
 call qr_readbits
 jc .bad
 cmp eax,7089          ; finite sanity limit for large character counts
 ja .bad
 mov [qr_count],eax
 cmp [qr_mode],1
 je .numeric
 cmp [qr_mode],2
 je .alphanumeric
 cmp [qr_mode],4
 je .bytes
 cmp [qr_mode],8
 je .kanji
 jmp .hanzi

.numeric:
 cmp [qr_count],3
 jb .numeric_rem
 mov ecx,10
 call qr_readbits
 jc .bad
 cmp eax,999
 ja .bad
 mov [qr_value],eax
 xor edx,edx
 mov ebx,100
 div ebx
 add al,'0'
 call qr_emit
 jc .full
 mov eax,[qr_value]
 xor edx,edx
 mov ebx,100
 div ebx
 mov eax,edx
 xor edx,edx
 mov ebx,10
 div ebx
 mov [qr_tmp],edx
 add al,'0'
 call qr_emit
 jc .full
 mov eax,[qr_tmp]
 add al,'0'
 call qr_emit
 jc .full
 sub [qr_count],3
 jmp .numeric
.numeric_rem:
 cmp [qr_count],0
 je .next_mode
 cmp [qr_count],2
 jne .numeric_one
 mov ecx,7
 call qr_readbits
 jc .bad
 cmp eax,99
 ja .bad
 xor edx,edx
 mov ebx,10
 div ebx
 mov [qr_tmp],edx
 add al,'0'
 call qr_emit
 jc .full
 mov eax,[qr_tmp]
 add al,'0'
 call qr_emit
 jc .full
 jmp .next_mode
.numeric_one:
 mov ecx,4
 call qr_readbits
 jc .bad
 cmp eax,9
 ja .bad
 add al,'0'
 call qr_emit
 jc .full
 jmp .next_mode

.alphanumeric:
 cmp [qr_count],2
 jb .alpha_odd
 mov ecx,11
 call qr_readbits
 jc .bad
 cmp eax,2024
 ja .bad
 xor edx,edx
 mov ecx,45
 div ecx
 mov [qr_value],edx
 movzx eax,byte [qr_alnum+eax]
 call qr_emit_alnum
 jc .full
 mov eax,[qr_value]
 mov al,[qr_alnum+eax]
 call qr_emit_alnum
 jc .full
 sub [qr_count],2
 jmp .alphanumeric
.alpha_odd:
 cmp [qr_count],0
 je .alpha_finish
 mov ecx,6
 call qr_readbits
 jc .bad
 cmp eax,44
 ja .bad
 mov al,[qr_alnum+eax]
 call qr_emit_alnum
 jc .full
.alpha_finish:
 call qr_flush_alnum
 jc .full
 jmp .next_mode

.bytes:
 cmp [qr_count],0
 je .next_mode
 mov ecx,8
 call qr_readbits
 jc .bad
 call qr_emit
 jc .full
 dec [qr_count]
 jmp .bytes

.kanji:
 cmp [qr_count],0
 je .next_mode
 mov ecx,13
 call qr_readbits
 jc .bad
 xor edx,edx
 mov ebx,0C0h
 div ebx
 shl eax,8
 add eax,edx
 cmp eax,1F00h
 jae .sjis_hi
 add eax,8140h
 jmp .sjis_emit
.sjis_hi:
 add eax,0C140h
.sjis_emit:
 mov [qr_value],eax
 shr eax,8
 call qr_emit
 jc .full
 mov eax,[qr_value]
 call qr_emit
 jc .full
 dec [qr_count]
 jmp .kanji

.hanzi:
 cmp [qr_count],0
 je .next_mode
 mov ecx,13
 call qr_readbits
 jc .bad
 xor edx,edx
 mov ebx,60h
 div ebx
 shl eax,8
 add eax,edx
 cmp eax,0A00h
 jae .gb_hi
 add eax,0A1A1h
 jmp .gb_emit
.gb_hi:
 add eax,0A6A1h
.gb_emit:
 mov [qr_value],eax
 shr eax,8
 call qr_emit
 jc .full
 mov eax,[qr_value]
 call qr_emit
 jc .full
 dec [qr_count]
 jmp .hanzi
.done:
 call qr_flush_alnum
 jc .full
 xor eax,eax
 clc
 ret
.full:
 mov eax,-5
 stc
 ret
.bad:
 mov eax,-4
 stc
 ret
