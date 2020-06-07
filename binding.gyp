{
    "targets": [
        {
            "target_name": "nexpress",
            "sources": [
                "src/nexpress.cc",
                "src/application.cc",
                "src/embeddedHttp.cc",
                "src/httpConnection.cc",
                "src/middleware.cc",
                "src/pathRegexp.cc",
                "src/request.cc",
                "src/response.cc",
                "src/router.cc",
                "src/next.cc",
                "src/pathRegexp.cc",
                "deps/picohttpparser/picohttpparser.c",
            ],
            "include_dirs" : [
                "deps/uvw",
                "deps/picohttpparser"
            ],
            "cflags_cc": [
                "-std=c++17",
                "-fexceptions"
            ],
            'xcode_settings': {
                'OTHER_CPLUSPLUSFLAGS': [
                    "-std=c++17",
                    "-fexceptions"
                ],
            }
        }
    ]
}