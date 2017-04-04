{
    "targets": [
        {
            "target_name": "ba2tk",
            "includes": [
                "auto.gypi"
            ],
            "sources": [
                "bsatk/src/ba2archive.cpp",
                "bsatk/src/ba2exception.cpp",
                "bsatk/src/ba2types.cpp",
                "index.cpp"
            ],
            "include_dirs": [
                "./zlib/include"
            ],
            "cflags!": ["-fno-exceptions"],
            "cflags_cc!": ["-fno-exceptions"],
            "conditions": [
                [
                    'OS=="win"',
                    {
                        "defines!": [
                            "_HAS_EXCEPTIONS=0"
                        ],
                        "libraries": [
                            "-l../zlib/win32/zlibstatic.lib"
                        ],
                        "msvs_settings": {
                            "VCCLCompilerTool": {
                                "ExceptionHandling": 1
                            }
                        }
                    }
                ],
                [
                    'OS=="mac"',
                    {
                        "xcode_settings": {
                            "GCC_ENABLE_CPP_EXCEPTIONS": "YES"
                        }
                    }
                ]
            ]
        }
    ],
    "includes": [
        "auto-top.gypi"
    ]
}
