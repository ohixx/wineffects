// Embeds the RNNoise 0.2 model (third_party/rnnoise/weights_blob.bin) into the executable.
// GNU assembler directive, so this file needs GCC / MinGW.
#ifndef RNNOISE_BLOB_PATH
#error "RNNOISE_BLOB_PATH must point to weights_blob.bin"
#endif

__asm__(
    ".section .rdata,\"dr\"\n"
    ".balign 64\n"
    ".globl rnnoise_blob_start\n"
    "rnnoise_blob_start:\n"
    ".incbin \"" RNNOISE_BLOB_PATH "\"\n"
    ".globl rnnoise_blob_end\n"
    "rnnoise_blob_end:\n"
    ".byte 0\n"
    ".text\n");
