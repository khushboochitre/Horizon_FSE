; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -mtriple=riscv32 -mattr=+d -verify-machineinstrs < %s \
; RUN:   | FileCheck -check-prefix=RV32IFD %s
; RUN: llc -mtriple=riscv64 -mattr=+d -verify-machineinstrs < %s \
; RUN:   | FileCheck -check-prefix=RV64IFD %s

define float @fcvt_s_d(double %a) nounwind {
; RV32IFD-LABEL: fcvt_s_d:
; RV32IFD:       # %bb.0:
; RV32IFD-NEXT:    addi sp, sp, -16
; RV32IFD-NEXT:    sw a0, 8(sp)
; RV32IFD-NEXT:    sw a1, 12(sp)
; RV32IFD-NEXT:    fld ft0, 8(sp)
; RV32IFD-NEXT:    fcvt.s.d ft0, ft0
; RV32IFD-NEXT:    fmv.x.w a0, ft0
; RV32IFD-NEXT:    addi sp, sp, 16
; RV32IFD-NEXT:    ret
;
; RV64IFD-LABEL: fcvt_s_d:
; RV64IFD:       # %bb.0:
; RV64IFD-NEXT:    fmv.d.x ft0, a0
; RV64IFD-NEXT:    fcvt.s.d ft0, ft0
; RV64IFD-NEXT:    fmv.x.w a0, ft0
; RV64IFD-NEXT:    ret
  %1 = fptrunc double %a to float
  ret float %1
}

define double @fcvt_d_s(float %a) nounwind {
; RV32IFD-LABEL: fcvt_d_s:
; RV32IFD:       # %bb.0:
; RV32IFD-NEXT:    addi sp, sp, -16
; RV32IFD-NEXT:    fmv.w.x ft0, a0
; RV32IFD-NEXT:    fcvt.d.s ft0, ft0
; RV32IFD-NEXT:    fsd ft0, 8(sp)
; RV32IFD-NEXT:    lw a0, 8(sp)
; RV32IFD-NEXT:    lw a1, 12(sp)
; RV32IFD-NEXT:    addi sp, sp, 16
; RV32IFD-NEXT:    ret
;
; RV64IFD-LABEL: fcvt_d_s:
; RV64IFD:       # %bb.0:
; RV64IFD-NEXT:    fmv.w.x ft0, a0
; RV64IFD-NEXT:    fcvt.d.s ft0, ft0
; RV64IFD-NEXT:    fmv.x.d a0, ft0
; RV64IFD-NEXT:    ret
  %1 = fpext float %a to double
  ret double %1
}

; For RV64D, fcvt.l.d is semantically equivalent to fcvt.w.d in this case
; because fptosi will produce poison if the result doesn't fit into an i32.
define i32 @fcvt_w_d(double %a) nounwind {
; RV32IFD-LABEL: fcvt_w_d:
; RV32IFD:       # %bb.0:
; RV32IFD-NEXT:    addi sp, sp, -16
; RV32IFD-NEXT:    sw a0, 8(sp)
; RV32IFD-NEXT:    sw a1, 12(sp)
; RV32IFD-NEXT:    fld ft0, 8(sp)
; RV32IFD-NEXT:    fcvt.w.d a0, ft0, rtz
; RV32IFD-NEXT:    addi sp, sp, 16
; RV32IFD-NEXT:    ret
;
; RV64IFD-LABEL: fcvt_w_d:
; RV64IFD:       # %bb.0:
; RV64IFD-NEXT:    fmv.d.x ft0, a0
; RV64IFD-NEXT:    fcvt.l.d a0, ft0, rtz
; RV64IFD-NEXT:    ret
  %1 = fptosi double %a to i32
  ret i32 %1
}

; For RV64D, fcvt.lu.d is semantically equivalent to fcvt.wu.d in this case
; because fptosi will produce poison if the result doesn't fit into an i32.
define i32 @fcvt_wu_d(double %a) nounwind {
; RV32IFD-LABEL: fcvt_wu_d:
; RV32IFD:       # %bb.0:
; RV32IFD-NEXT:    addi sp, sp, -16
; RV32IFD-NEXT:    sw a0, 8(sp)
; RV32IFD-NEXT:    sw a1, 12(sp)
; RV32IFD-NEXT:    fld ft0, 8(sp)
; RV32IFD-NEXT:    fcvt.wu.d a0, ft0, rtz
; RV32IFD-NEXT:    addi sp, sp, 16
; RV32IFD-NEXT:    ret
;
; RV64IFD-LABEL: fcvt_wu_d:
; RV64IFD:       # %bb.0:
; RV64IFD-NEXT:    fmv.d.x ft0, a0
; RV64IFD-NEXT:    fcvt.lu.d a0, ft0, rtz
; RV64IFD-NEXT:    ret
  %1 = fptoui double %a to i32
  ret i32 %1
}

define double @fcvt_d_w(i32 %a) nounwind {
; RV32IFD-LABEL: fcvt_d_w:
; RV32IFD:       # %bb.0:
; RV32IFD-NEXT:    addi sp, sp, -16
; RV32IFD-NEXT:    fcvt.d.w ft0, a0
; RV32IFD-NEXT:    fsd ft0, 8(sp)
; RV32IFD-NEXT:    lw a0, 8(sp)
; RV32IFD-NEXT:    lw a1, 12(sp)
; RV32IFD-NEXT:    addi sp, sp, 16
; RV32IFD-NEXT:    ret
;
; RV64IFD-LABEL: fcvt_d_w:
; RV64IFD:       # %bb.0:
; RV64IFD-NEXT:    fcvt.d.w ft0, a0
; RV64IFD-NEXT:    fmv.x.d a0, ft0
; RV64IFD-NEXT:    ret
  %1 = sitofp i32 %a to double
  ret double %1
}

define double @fcvt_d_wu(i32 %a) nounwind {
; RV32IFD-LABEL: fcvt_d_wu:
; RV32IFD:       # %bb.0:
; RV32IFD-NEXT:    addi sp, sp, -16
; RV32IFD-NEXT:    fcvt.d.wu ft0, a0
; RV32IFD-NEXT:    fsd ft0, 8(sp)
; RV32IFD-NEXT:    lw a0, 8(sp)
; RV32IFD-NEXT:    lw a1, 12(sp)
; RV32IFD-NEXT:    addi sp, sp, 16
; RV32IFD-NEXT:    ret
;
; RV64IFD-LABEL: fcvt_d_wu:
; RV64IFD:       # %bb.0:
; RV64IFD-NEXT:    fcvt.d.wu ft0, a0
; RV64IFD-NEXT:    fmv.x.d a0, ft0
; RV64IFD-NEXT:    ret
  %1 = uitofp i32 %a to double
  ret double %1
}

define i64 @fcvt_l_d(double %a) nounwind {
; RV32IFD-LABEL: fcvt_l_d:
; RV32IFD:       # %bb.0:
; RV32IFD-NEXT:    addi sp, sp, -16
; RV32IFD-NEXT:    sw ra, 12(sp)
; RV32IFD-NEXT:    call __fixdfdi
; RV32IFD-NEXT:    lw ra, 12(sp)
; RV32IFD-NEXT:    addi sp, sp, 16
; RV32IFD-NEXT:    ret
;
; RV64IFD-LABEL: fcvt_l_d:
; RV64IFD:       # %bb.0:
; RV64IFD-NEXT:    fmv.d.x ft0, a0
; RV64IFD-NEXT:    fcvt.l.d a0, ft0, rtz
; RV64IFD-NEXT:    ret
  %1 = fptosi double %a to i64
  ret i64 %1
}

define i64 @fcvt_lu_d(double %a) nounwind {
; RV32IFD-LABEL: fcvt_lu_d:
; RV32IFD:       # %bb.0:
; RV32IFD-NEXT:    addi sp, sp, -16
; RV32IFD-NEXT:    sw ra, 12(sp)
; RV32IFD-NEXT:    call __fixunsdfdi
; RV32IFD-NEXT:    lw ra, 12(sp)
; RV32IFD-NEXT:    addi sp, sp, 16
; RV32IFD-NEXT:    ret
;
; RV64IFD-LABEL: fcvt_lu_d:
; RV64IFD:       # %bb.0:
; RV64IFD-NEXT:    fmv.d.x ft0, a0
; RV64IFD-NEXT:    fcvt.lu.d a0, ft0, rtz
; RV64IFD-NEXT:    ret
  %1 = fptoui double %a to i64
  ret i64 %1
}

define i64 @fmv_x_d(double %a, double %b) nounwind {
; RV32IFD-LABEL: fmv_x_d:
; RV32IFD:       # %bb.0:
; RV32IFD-NEXT:    addi sp, sp, -16
; RV32IFD-NEXT:    sw a2, 0(sp)
; RV32IFD-NEXT:    sw a3, 4(sp)
; RV32IFD-NEXT:    fld ft0, 0(sp)
; RV32IFD-NEXT:    sw a0, 0(sp)
; RV32IFD-NEXT:    sw a1, 4(sp)
; RV32IFD-NEXT:    fld ft1, 0(sp)
; RV32IFD-NEXT:    fadd.d ft0, ft1, ft0
; RV32IFD-NEXT:    fsd ft0, 8(sp)
; RV32IFD-NEXT:    lw a0, 8(sp)
; RV32IFD-NEXT:    lw a1, 12(sp)
; RV32IFD-NEXT:    addi sp, sp, 16
; RV32IFD-NEXT:    ret
;
; RV64IFD-LABEL: fmv_x_d:
; RV64IFD:       # %bb.0:
; RV64IFD-NEXT:    fmv.d.x ft0, a1
; RV64IFD-NEXT:    fmv.d.x ft1, a0
; RV64IFD-NEXT:    fadd.d ft0, ft1, ft0
; RV64IFD-NEXT:    fmv.x.d a0, ft0
; RV64IFD-NEXT:    ret
  %1 = fadd double %a, %b
  %2 = bitcast double %1 to i64
  ret i64 %2
}

define double @fcvt_d_l(i64 %a) nounwind {
; RV32IFD-LABEL: fcvt_d_l:
; RV32IFD:       # %bb.0:
; RV32IFD-NEXT:    addi sp, sp, -16
; RV32IFD-NEXT:    sw ra, 12(sp)
; RV32IFD-NEXT:    call __floatdidf
; RV32IFD-NEXT:    lw ra, 12(sp)
; RV32IFD-NEXT:    addi sp, sp, 16
; RV32IFD-NEXT:    ret
;
; RV64IFD-LABEL: fcvt_d_l:
; RV64IFD:       # %bb.0:
; RV64IFD-NEXT:    fcvt.d.l ft0, a0
; RV64IFD-NEXT:    fmv.x.d a0, ft0
; RV64IFD-NEXT:    ret
  %1 = sitofp i64 %a to double
  ret double %1
}

define double @fcvt_d_lu(i64 %a) nounwind {
; RV32IFD-LABEL: fcvt_d_lu:
; RV32IFD:       # %bb.0:
; RV32IFD-NEXT:    addi sp, sp, -16
; RV32IFD-NEXT:    sw ra, 12(sp)
; RV32IFD-NEXT:    call __floatundidf
; RV32IFD-NEXT:    lw ra, 12(sp)
; RV32IFD-NEXT:    addi sp, sp, 16
; RV32IFD-NEXT:    ret
;
; RV64IFD-LABEL: fcvt_d_lu:
; RV64IFD:       # %bb.0:
; RV64IFD-NEXT:    fcvt.d.lu ft0, a0
; RV64IFD-NEXT:    fmv.x.d a0, ft0
; RV64IFD-NEXT:    ret
  %1 = uitofp i64 %a to double
  ret double %1
}

define double @fmv_d_x(i64 %a, i64 %b) nounwind {
; Ensure fmv.w.x is generated even for a soft double calling convention
; RV32IFD-LABEL: fmv_d_x:
; RV32IFD:       # %bb.0:
; RV32IFD-NEXT:    addi sp, sp, -32
; RV32IFD-NEXT:    sw a3, 20(sp)
; RV32IFD-NEXT:    sw a2, 16(sp)
; RV32IFD-NEXT:    sw a1, 28(sp)
; RV32IFD-NEXT:    sw a0, 24(sp)
; RV32IFD-NEXT:    fld ft0, 16(sp)
; RV32IFD-NEXT:    fld ft1, 24(sp)
; RV32IFD-NEXT:    fadd.d ft0, ft1, ft0
; RV32IFD-NEXT:    fsd ft0, 8(sp)
; RV32IFD-NEXT:    lw a0, 8(sp)
; RV32IFD-NEXT:    lw a1, 12(sp)
; RV32IFD-NEXT:    addi sp, sp, 32
; RV32IFD-NEXT:    ret
;
; RV64IFD-LABEL: fmv_d_x:
; RV64IFD:       # %bb.0:
; RV64IFD-NEXT:    fmv.d.x ft0, a0
; RV64IFD-NEXT:    fmv.d.x ft1, a1
; RV64IFD-NEXT:    fadd.d ft0, ft0, ft1
; RV64IFD-NEXT:    fmv.x.d a0, ft0
; RV64IFD-NEXT:    ret
  %1 = bitcast i64 %a to double
  %2 = bitcast i64 %b to double
  %3 = fadd double %1, %2
  ret double %3
}
