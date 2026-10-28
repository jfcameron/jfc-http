# jfc-http
Concurrent http client. Provides a null, libcurl and emscripten fetch implementation and selects which one at generation time based on cmake flags.

API documentation: https://jfcameron.github.io/jfc-http/

## Getting Started
Install cmake 3.21 or greater, and a c++20 compatible compiler 
```
# initialize the tree
git submodule update --init --recursive

# generate the script to build the library on linux using gcc
cmake --preset linux-gcc

# build the library
cmake --build --preset linux-gcc

# run the test suite
ctest --preset linux-gcc
```
See `CMakePresets.json` for the full list of supported platforms and compilers. See `.github/workflows/ci.yml` to see exactly how CI runners build it on different platforms.

On Windows and Android, curl is built from `extern/curl` (`JFCHTTP_VENDORED_CURL`): with Schannel on Windows, and with mbedTLS from `extern/mbedtls` on Android, trusting the system's certificates. Elsewhere the system's libcurl is used. iOS is not supported yet: curl has no TLS there that does not mean building OpenSSL or shipping certificates.
