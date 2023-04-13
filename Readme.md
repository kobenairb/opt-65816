# Assembly code Optimizer for the WDC 65816

- [Assembly code Optimizer for the WDC 65816](#assembly-code-optimizer-for-the-wdc-65816)
  - [About](#about)
  - [Getting Started](#getting-started)
    - [Build it](#build-it)
    - [Generate the documentation](#generate-the-documentation)
    - [Memory checker using valgrind](#memory-checker-using-valgrind)
    - [Usage](#usage)
  - [Authors](#authors)
  - [License](#license)
  - [Acknowledgements](#acknowledgements)

## About

Assembly code optimizer for the [WDC 65816](https://en.wikipedia.org/wiki/WDC_65C816) processor produced by the [816-tcc](https://github.com/alekmaul/tcc) *(65816 Tiny C Compiler)*.

This project is a **C** port of the `816-opt` Python tool.

## Getting Started

### Build it

Compile it using the `make` command.

```
make
```
### Generate the documentation

Generate the documentation in `doc/html` like this.

```
make doc
```

### Memory checker using valgrind

If you want to modify the code, you can use the following `make` command to test memory leaks.

```
make valgrind
```

It will launch `opt-65816` with **valgrind** and will iterate over the `samples/*.ps` files *(these ASM files come from the examples available in the [pvsneslib](https://github.com/alekmaul/pvsneslib) project)*.


### Usage

Just give the ASM file to optimize as argument to `opt-65816`.

```
opt-65816 /path/to/your/asm/file
```

Or by using the `stdin`.

```
cat /path/to/your/asm/file | opt-65816
```

## Authors

- [@Kobenairb](https://github.com/kobenairb)

## License

This project is released under the GNU Public License.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

## Acknowledgements

The main contributors of the `816-opt` python tool:

- Ulrich Hecht
- Mic_
- [@Alekmaul](https://github.com/alekmaul)

And all the other contributors that I forget to name.
