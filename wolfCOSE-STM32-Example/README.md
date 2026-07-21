# wolfCOSE STM32 Examples

STM32Cube examples that run wolfCOSE (CBOR/COSE on wolfCrypt) on hardware.

wolfCOSE is delivered as an STM32Cube pack, `I-CUBE-wolfCOSE`, alongside the
wolfSSL pack it depends on. Install both from
[wolfssl.com/files/ide](https://www.wolfssl.com/files/ide/), then follow a board
example below.

## Boards

- [NUCLEO-H563ZI](NUCLEO-H563ZI/README.md): runs the wolfCOSE `COSE_Sign1` ES256
  self test and prints the result over the ST-LINK virtual COM port.

See the [wolfCOSE STM32Cube wiki page](https://github.com/wolfSSL/wolfCOSE/wiki/STM32Cube)
for installing the pack.
