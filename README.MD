# EASy68K Assembler for Linux

This project is a port of the EASy68K 68000 Assembler to run on Linux.

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Usage](#usage)
- [Compilation](#compilation)
- [License](#license)

## Introduction

The EASy68K Assembler is a powerful tool for 68000 assembly language programming. This project is a Linux-compatible version, providing the same functionality without the editor component.

## Features

- Assembles 68000 assembly code (Easy68K syntax).
- 32 and 64 bit support.
- Linux compatibility.

## Usage

```bash
Rigel68K sourceFile.x68 [output]

You can omit the output name; it will default to "genesis."

You need nothing more than g++ to build this project.
Compilation

Clone the repository:

```bash
git clone https://github.com/your-username/easy68k-linux.git
./build.sh

## License

Modified EASy68K 68000 Assembler for Linux
Based on EASy68K 68000 Editor/Assembler/Simulator
Original Copyright (C) 2002-2018 Charles Kelly and Tim Larson
Linux Modifications Copyright (C) Mouloud Agaoua 2023

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

For the original software by Charles Kelly, visit www.easy68k.com.
