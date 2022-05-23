project(dem-transpose)
set(CMAKE_BUILD_TYPE Debug)
add_executable(dem_transpose
    buf.c
    buf.h
    decode.c
    encode.c
    encode.h
    main.c
    main.h
    msglen.c
    msglen.h
    private.h
    quakedef.h
    tp_private.h
    transpose.h
)
