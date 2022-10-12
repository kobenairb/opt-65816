# Assembly code Optimizer for WDC65816


[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)


  - [About](#about)
  - [Getting Started](#getting-started)
    - [Build it](#build-it)
  - [Usage](#usage)
  - [Authors](#authors)
  - [License](#license)
  - [Acknowledgements](#acknowledgements)

## About

Assembly code optimizer for the WDC65816 processor produced
by the 65816 Tiny C Compiler (816-tcc) developed by [@AlekMaul](https://github.com/alekmaul).

This library is a `C` port of the `816-opt` Python tool developed by [@nArnoSNES](https://github.com/arnosnes).

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See [deployment](#deployment) for notes on how to deploy the project on a live system.

### Build it

Just compile it using the `make` command.

```
make
```

## Usage

Just give the ASM file to optimze as argument to `opt-65816`.

```
opt_65816 /path/to/your/asm/file
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
