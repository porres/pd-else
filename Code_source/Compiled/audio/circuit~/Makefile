# library name
lib.name = circuit~

XINCLUDE = \
    -I./Libraries \
    -I./Libraries/AMD \
    -I./Libraries/BTF \
    -I./Libraries/KLU \
    -I./Libraries/libsamplerate \
    -I./Libraries/SuiteSparse_config \
    -I./Source \
    ${empty}

cflags += ${XINCLUDE} -I .

define forDarwin
    cflags += -mmacosx-version-min=10.9
    cxx.flags += -stdlib=libc++ -std=gnu++17 -fPIC
endef

define forLinux
    cflags += -std=c++1z
endef

define forWindows
    cflags += -std=c++1z
endef
    
circuit~.class.sources = \
    ./Libraries/AMD/amd_1.c \
    ./Libraries/AMD/amd_2.c \
    ./Libraries/AMD/amd_aat.c \
    ./Libraries/AMD/amd_control.c \
    ./Libraries/AMD/amd_defaults.c \
    ./Libraries/AMD/amd_dump.c \
    ./Libraries/AMD/amd_info.c \
    ./Libraries/AMD/amd_l1.c \
    ./Libraries/AMD/amd_l2.c \
    ./Libraries/AMD/amd_l_aat.c \
    ./Libraries/AMD/amd_l_control.c \
    ./Libraries/AMD/amd_l_defaults.c \
    ./Libraries/AMD/amd_l_dump.c \
    ./Libraries/AMD/amd_l_info.c \
    ./Libraries/AMD/amd_l_order.c \
    ./Libraries/AMD/amd_l_postorder.c \
    ./Libraries/AMD/amd_l_post_tree.c \
    ./Libraries/AMD/amd_l_preprocess.c \
    ./Libraries/AMD/amd_l_valid.c \
    ./Libraries/AMD/amd_order.c \
    ./Libraries/AMD/amd_postorder.c \
    ./Libraries/AMD/amd_post_tree.c \
    ./Libraries/AMD/amd_preprocess.c \
    ./Libraries/AMD/amd_valid.c \
    ./Libraries/BTF/btf_l_maxtrans.c \
    ./Libraries/BTF/btf_l_order.c \
    ./Libraries/BTF/btf_l_strongcomp.c \
    ./Libraries/BTF/btf_maxtrans.c \
    ./Libraries/BTF/btf_order.c \
    ./Libraries/BTF/btf_strongcomp.c \
    ./Libraries/COLAMD/colamd.c \
    ./Libraries/COLAMD/colamd_l.c \
    ./Libraries/KLU/klu.c \
    ./Libraries/KLU/klu_analyze.c \
    ./Libraries/KLU/klu_analyze_given.c \
    ./Libraries/KLU/klu_defaults.c \
    ./Libraries/KLU/klu_diagnostics.c \
    ./Libraries/KLU/klu_dump.c \
    ./Libraries/KLU/klu_extract.c \
    ./Libraries/KLU/klu_factor.c \
    ./Libraries/KLU/klu_free_numeric.c \
    ./Libraries/KLU/klu_free_symbolic.c \
    ./Libraries/KLU/klu_kernel.c \
    ./Libraries/KLU/klu_l.c \
    ./Libraries/KLU/klu_l_analyze.c \
    ./Libraries/KLU/klu_l_analyze_given.c \
    ./Libraries/KLU/klu_l_defaults.c \
    ./Libraries/KLU/klu_l_diagnostics.c \
    ./Libraries/KLU/klu_l_dump.c \
    ./Libraries/KLU/klu_l_extract.c \
    ./Libraries/KLU/klu_l_factor.c \
    ./Libraries/KLU/klu_l_free_numeric.c \
    ./Libraries/KLU/klu_l_free_symbolic.c \
    ./Libraries/KLU/klu_l_kernel.c \
    ./Libraries/KLU/klu_l_memory.c \
    ./Libraries/KLU/klu_l_refactor.c \
    ./Libraries/KLU/klu_l_scale.c \
    ./Libraries/KLU/klu_l_solve.c \
    ./Libraries/KLU/klu_l_sort.c \
    ./Libraries/KLU/klu_l_tsolve.c \
    ./Libraries/KLU/klu_memory.c \
    ./Libraries/KLU/klu_refactor.c \
    ./Libraries/KLU/klu_scale.c \
    ./Libraries/KLU/klu_solve.c \
    ./Libraries/KLU/klu_sort.c \
    ./Libraries/KLU/klu_tsolve.c \
    ./Libraries/KLU/klu_z.c \
    ./Libraries/KLU/klu_zl.c \
    ./Libraries/KLU/klu_zl_diagnostics.c \
    ./Libraries/KLU/klu_zl_dump.c \
    ./Libraries/KLU/klu_zl_extract.c \
    ./Libraries/KLU/klu_zl_factor.c \
    ./Libraries/KLU/klu_zl_free_numeric.c \
    ./Libraries/KLU/klu_zl_kernel.c \
    ./Libraries/KLU/klu_zl_refactor.c \
    ./Libraries/KLU/klu_zl_scale.c \
    ./Libraries/KLU/klu_zl_solve.c \
    ./Libraries/KLU/klu_zl_sort.c \
    ./Libraries/KLU/klu_zl_tsolve.c \
    ./Libraries/KLU/klu_z_diagnostics.c \
    ./Libraries/KLU/klu_z_dump.c \
    ./Libraries/KLU/klu_z_extract.c \
    ./Libraries/KLU/klu_z_factor.c \
    ./Libraries/KLU/klu_z_free_numeric.c \
    ./Libraries/KLU/klu_z_kernel.c \
    ./Libraries/KLU/klu_z_refactor.c \
    ./Libraries/KLU/klu_z_scale.c \
    ./Libraries/KLU/klu_z_solve.c \
    ./Libraries/KLU/klu_z_sort.c \
    ./Libraries/KLU/klu_z_tsolve.c \
    ./Libraries/libsamplerate/samplerate.c \
    ./Libraries/libsamplerate/src_linear.c \
    ./Libraries/libsamplerate/src_sinc.c \
    ./Libraries/libsamplerate/src_zoh.c \
    ./Libraries/SuiteSparse_config/SuiteSparse_config.c \
    ./Source/Simulator.cpp \
    ./Source/circuit~.c \
    ${empty}

PDLIBBUILDER_DIR=../../../../pd-lib-builder
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
