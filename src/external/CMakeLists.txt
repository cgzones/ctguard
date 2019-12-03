add_library (external_dtl INTERFACE)
target_include_directories (external_dtl SYSTEM INTERFACE dtl-master)

add_library (external_sha2 STATIC sha2/sha2.c)
target_include_directories (external_sha2 SYSTEM INTERFACE sha2)
