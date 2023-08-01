# BWT-RLE Reverse Search for UNSW COMP9319 23T2 Assignment 2

## Introduction

This program performs reverse search on a BWT-RLE encoded text file. A BWT-RLE encoded text file is a text file that has been encoded using the Burrows-Wheeler Transform (BWT) and then Run-Length Encoding (RLE). This program searches all occurrences of a given pattern in the original text file and outputs the records containing the pattern in the original text file.

## Limitations

This assignment has some significant limitations:

- Original text file contains only 98 ASCII characters('\n','\r','\t','\0' and 94 printable characters)
- Original text file is less than 160MB
- RAM usage is limited to **13MB**
- Can create an index file no larger than input file

## Performance

In extreme test case(9000 matches, 444000 chars output), the program takes 0.38s to run on a SSD machine and 1.95s to run on a HDD machine.