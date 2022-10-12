# Assembly code Optimizer for WDC65816


[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)


- [Assembly code Optimizer for WDC65816](#assembly-code-optimizer-for-wdc65816)
  - [About](#about)
  - [Getting Started](#getting-started)
    - [Build it](#build-it)
    - [Usage](#usage)
  - [Authors](#authors)
  - [License](#license)
  - [Acknowledgements](#acknowledgements)

## About

Assembly code optimizer for the *WDC65816* processor produced
by the *65816 Tiny C Compiler* (`816-tcc`) developed by [@AlekMaul](https://github.com/alekmaul).

This library is a `C` port of the `816-opt` Python tool developed by [@nArnoSNES](https://github.com/arnosnes).

## Getting Started

### Build it

Just compile it using the `make` command.

```
make
```

### Usage

Just give the ASM file to optimize as argument to `opt-65816`.

```
opt-65816 /path/to/your/asm/file
```

Or by using the `stdin`.

```
cat /path/to/your/asm/file | opt_65816
```

## Authors

- [@Kobenairb](https://github.com/kobenairb)

## License

This project is released under the GNU Public License.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

## Acknowledgements

- [@nArnoSNES](https://github.com/arnosnes)
- [@AlekMaul](https://github.com/alekmaul)
